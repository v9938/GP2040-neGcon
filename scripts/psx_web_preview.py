"""Loopback-only preview of the existing configurator, with no hardware access."""

import argparse
import copy
import ctypes
import html
import ipaddress
import json
import mimetypes
import os
from pathlib import Path
import re
import socket
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlsplit


MAX_BODY = 2 * 1024 * 1024
BUTTONS = ('Up Down Left Right B1 B2 B3 B4 L1 R1 L2 R2 S1 S2 L3 R3 '
           'A1 A2 A3 A4 E1 E2 E3 E4 E5 E6 E7 E8 E9 E10 E11 E12 Fn').split()
PSX_BUTTONS = ('UP DOWN RIGHT LEFT CROSS CIRCLE R2 L2 SQUARE TRIANGLE '
               'R1 L1 SELECT START L3 R3').split()
SPA_ROUTES = {'/', '/settings', '/pin-mapping', '/add-ons', '/backup',
              '/reset-settings', '/input-macro', '/macro', '/analog-monitor'}


class StickSettings(ctypes.Structure):
    _fields_ = [(key, ctypes.c_uint32) for key in ('mode', 'invert', 'innerDeadzone', 'outerDeadzone')] + [
        ('smoothingFactor', ctypes.c_float), ('error', ctypes.c_uint32)] + [
        (key, ctypes.c_uint8) for key in ('smoothing', 'circularity', 'autoCalibrate', 'reserved')]


class AnalogSettings(ctypes.Structure):
    _fields_ = [('sticks', StickSettings * 2), ('pressureModes', ctypes.c_uint32 * 3),
                ('pressureInner', ctypes.c_uint32 * 3), ('pressureOuter', ctypes.c_uint32 * 3),
                ('jogconDeadzone', ctypes.c_uint32), ('jogconMax', ctypes.c_uint32),
                ('jogconForcebackEnabled', ctypes.c_uint8), ('reserved', ctypes.c_uint8 * 3)]


class AnalogInput(ctypes.Structure):
    _fields_ = [('generation', ctypes.c_uint32)] + [(key, ctypes.c_uint8) for key in
        ('lx', 'ly', 'rx', 'ry', 'pressureI', 'pressureII', 'pressureL', 'connected', 'analog', 'negcon', 'id', 'reserved')] + [
        ('jogPosition', ctypes.c_int32)] + [(key, ctypes.c_uint8) for key in ('jogcon', 'jogStatus', 'jogReady', 'jogReserved')]


class StickState(ctypes.Structure):
    _fields_ = [('xEma', ctypes.c_float), ('yEma', ctypes.c_float)] + [(key, ctypes.c_uint8) for key in
        ('xCenter', 'yCenter', 'calibrated', 'reserved')]


class AnalogOutput(ctypes.Structure):
    _fields_ = [(key, ctypes.c_uint16) for key in ('lx', 'ly', 'rx', 'ry')] + [
        (key, ctypes.c_uint8) for key in ('lt', 'rt', 'validityMask', 'reset')]


class AnalogState(ctypes.Structure):
    _fields_ = [('sticks', StickState * 2), ('lastGeneration', ctypes.c_uint32)] + [
        (key, ctypes.c_uint8) for key in ('lastId', 'lastConnected', 'lastAnalog', 'lastNegcon')] + [
        ('output', AnalogOutput), ('lastJogcon', ctypes.c_uint8), ('reserved', ctypes.c_uint8 * 3)]


class JogSettings(ctypes.Structure):
    _fields_ = [('deadzone', ctypes.c_uint32), ('max', ctypes.c_uint32), ('forceback', ctypes.c_uint8)]


class JogState(ctypes.Structure):
    _fields_ = [('position', ctypes.c_int32)] + [(key, ctypes.c_uint32) for key in
        ('stableSince', 'startTime', 'lastValidTime')] + [(key, ctypes.c_uint8) for key in
        ('motorCommand', 'motorPower', 'resetStatus', 'connected', 'ready', 'stable', 'hasValid', 'status')]


ANALOG_RANGES = {
    'jogconDeadzone': (0, 32766), 'jogconMax': (1, 32767), 'jogconForcebackEnabled': (0, 1),
    **{f'analogAdc{n}{key}': (1, 2) if key == 'Mode' else (0, 3) for n in (1, 2) for key in ('Mode', 'Invert')},
    **{f'{key}{suffix}': limits for suffix in ('', '2') for key, limits in {
        'forced_circularity': (0, 1), 'inner_deadzone': (0, 100), 'outer_deadzone': (0, 100),
        'auto_calibrate': (0, 1), 'analog_smoothing': (0, 1), 'smoothing_factor': (0, 100),
        'analog_error': (821, 1000)}.items()},
    **{f'negcon{key}{field}': (0, 14) if field == 'Mode' else (0, 255)
       for key in ('I', 'II', 'L') for field in ('Mode', 'Threshold')},
    **{f'negcon{key}{field}': (0, 1) if field == 'DigitalEnabled' else (0, 100)
       for key in ('I', 'II', 'L') for field in ('DigitalEnabled', 'InnerDeadzone', 'OuterDeadzone')},
}
ERROR_RATES = {1000, 990, 979, 969, 958, 946, 934, 922, 911, 900, 890, 876, 863, 848, 834, 821}


def validate_gamepad_modes(options):
    for key in ('inputMode', 'inputModeB1', 'inputModeB2', 'inputModeB3', 'inputModeB4',
                'inputModeL1', 'inputModeL2', 'inputModeR1', 'inputModeR2'):
        if key in options:
            value = options[key]
            low = 0 if key == 'inputMode' else -1
            if type(value) is not int or not low <= value <= 16:
                raise ValueError(f'{key}: unsupported input mode')


def validate_analog_options(options):
    if not isinstance(options, dict) or set(options) != set(ANALOG_RANGES):
        raise ValueError('draft must contain only the complete editable analog allowlist')
    for key, (low, high) in ANALOG_RANGES.items():
        value = options[key]
        if type(value) not in (int, float) or not low <= value <= high:
            raise ValueError(f'{key}: invalid finite value')
        if key.endswith('DigitalEnabled') and type(value) is not int:
            raise ValueError(f'{key}: integer 0 or 1 required')
        if not key.startswith('smoothing_factor') and (type(value) is not int and not value.is_integer()):
            raise ValueError(f'{key}: integer required')
        if key.startswith('analog_error') and value not in ERROR_RATES:
            raise ValueError(f'{key}: unsupported error rate')
    if options['analogAdc1Mode'] == options['analogAdc2Mode']:
        raise ValueError('PSX stick outputs must be different')
    if options['jogconDeadzone'] >= options['jogconMax']:
        raise ValueError('0 <= jogconDeadzone < jogconMax <= 32767 required')
    for suffix in ('', '2'):
        if options[f'inner_deadzone{suffix}'] >= options[f'outer_deadzone{suffix}']:
            raise ValueError('outer deadzone must exceed inner deadzone')
    for key in ('I', 'II', 'L'):
        if options[f'negcon{key}InnerDeadzone'] >= options[f'negcon{key}OuterDeadzone']:
            raise ValueError(f'neGcon {key}: outer deadzone must exceed inner deadzone')


def dll_settings(options):
    validate_analog_options(options)
    settings = AnalogSettings()
    for index, suffix in enumerate(('', '2')):
        stick = settings.sticks[index]
        stick.mode = int(options[f'analogAdc{index + 1}Mode'])
        stick.invert = int(options[f'analogAdc{index + 1}Invert'])
        for attribute, key in (('innerDeadzone', 'inner_deadzone'), ('outerDeadzone', 'outer_deadzone'),
                               ('error', 'analog_error'), ('smoothing', 'analog_smoothing'),
                               ('circularity', 'forced_circularity'), ('autoCalibrate', 'auto_calibrate')):
            setattr(stick, attribute, int(options[f'{key}{suffix}']))
        stick.smoothingFactor = options[f'smoothing_factor{suffix}']
    for index, key in enumerate(('I', 'II', 'L')):
        settings.pressureModes[index] = int(options[f'negcon{key}Mode'])
        settings.pressureInner[index] = int(options[f'negcon{key}InnerDeadzone'])
        settings.pressureOuter[index] = int(options[f'negcon{key}OuterDeadzone'])
    settings.jogconDeadzone = int(options['jogconDeadzone'])
    settings.jogconMax = int(options['jogconMax'])
    settings.jogconForcebackEnabled = int(options['jogconForcebackEnabled'])
    return settings


def analog_session(payload):
    session = payload.get('session')
    if not isinstance(session, str) or not re.fullmatch(r'[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}', session):
        raise ValueError('valid UUID session required')
    return session


def loopback_host(value):
    if value.lower() == 'localhost':
        return '127.0.0.1'
    try:
        if ipaddress.ip_address(value).is_loopback:
            return value
    except ValueError:
        pass
    raise argparse.ArgumentTypeError('host must be a loopback IP or localhost')


def defaults():
    mappings = {'profileLabel': 'Profile 1', 'enabled': True}
    actions = [1, 2, 4, 3, 5, 6, 12, 11, 7, 8, 10, 9, 13, 14, 17, 18]
    for pin in range(30):
        mappings[f'pin{pin:02}'] = {
            'action': actions[pin - 8] if 8 <= pin <= 23 else -10,
            'customButtonMask': 0, 'customDpadMask': 0,
        }
    profiles = []
    for number in range(2, 7):
        profile = copy.deepcopy(mappings)
        profile.update(profileLabel=f'Profile {number}', enabled=False)
        profiles.append(profile)
    gamepad = dict(dpadMode=0, inputMode=0, inputDeviceType=0, socdMode=2,
                   switchTpShareForDs4=0, forcedSetupMode=0, lockHotkeys=0,
                   fourWayMode=0, fnButtonPin=-1, profileNumber=1,
                   debounceDelay=5, ps4AuthType=0, ps5AuthType=0,
                   xinputAuthType=0, ps4ControllerIDMode=0, usbDescOverride=0,
                   usbDescProduct='GP2040-CE Preview',
                   usbDescManufacturer='Preview only', usbDescVersion='1.0',
                   usbOverrideID=0, usbVendorID='10C4', usbProductID='82C0',
                   miniMenuGamepadInput=0)
    gamepad.update({f'inputMode{button}': -1 for button in
                    ('B1', 'B2', 'B3', 'B4', 'L1', 'L2', 'R1', 'R2')})
    gamepad.update({f'hotkey{n:02}': {'auxMask': 0, 'buttonsMask': 0, 'action': 0}
                    for n in range(1, 17)})
    peripheral = {}
    for n in range(2):
        peripheral[f'i2c{n}'] = dict(enabled=0, sda=-1, scl=-1, speed=400000)
        peripheral[f'spi{n}'] = dict(enabled=0, rx=-1, cs=-1, sck=-1, tx=-1)
    peripheral['usb0'] = dict(enabled=1, dp=6, enable5v=-1, order=0)
    addons = dict(AnalogInputEnabled=1, analogAdc1Mode=1, analogAdc2Mode=2,
                  jogconDeadzone=0, jogconMax=100, jogconForcebackEnabled=1,
                  negconIMode=12, negconIIMode=11, negconLMode=0,
                  negconIThreshold=128, negconIIThreshold=128,
                  negconLThreshold=240, BoardLedAddonEnabled=1,
                  onBoardLedMode=1, ReactiveLEDAddonEnabled=1,
                  BootselButtonAddonEnabled=0, bootselButtonMap=0,
                  ReverseInputEnabled=0, reversePin=-1, reversePinLED=-1,
                  KeyboardHostAddonEnabled=0, GamepadUSBHostAddonEnabled=0,
                  InputMacroAddonEnabled=0, keyboardHostMouseLeft=0,
                  keyboardHostMouseMiddle=0, keyboardHostMouseRight=0,
                  keyboardHostMouseSensitivity=50, keyboardHostMouseMovement=0,
                  keyboardHostMap={b: 0 for b in BUTTONS}, turboLedColor=0)
    for direction in ('Up', 'Down', 'Left', 'Right'):
        addons[f'reverseAction{direction}'] = 1
    for key in ('I', 'II', 'L'):
        addons.update({f'negcon{key}DigitalEnabled': 1, f'negcon{key}InnerDeadzone': 0,
                       f'negcon{key}OuterDeadzone': 100})
    for suffix in ('', '2'):
        addons.update({f'{key}{suffix}': value for key, value in {
            'forced_circularity': 0, 'inner_deadzone': 5, 'outer_deadzone': 95,
            'auto_calibrate': 0, 'analog_smoothing': 0, 'smoothing_factor': 5,
            'analog_error': 1000}.items()})
    for n in (1, 2):
        addons.update({f'analogAdc{n}Invert': 0, f'analogAdc{n}PinX': -1,
                       f'analogAdc{n}PinY': -1})
    macro = dict(enabled=0, exclusive=1, interruptible=1, showFrames=1,
                 macroType=1, useMacroTriggerButton=0, macroTriggerButton=0,
                 macroLabel='', macroInputs=[])
    return {
        'GamepadOptions': gamepad, 'PinMappings': mappings,
        'ProfileOptions': {'alternativePinMappings': profiles},
        'PeripheralOptions': {'peripheral': peripheral}, 'AddonsOptions': addons,
        'MacroAddonOptions': {'macroList': [copy.deepcopy(macro) for _ in range(6)],
                              'macroBoardLedEnabled': 0},
        'KeyMappings': {b: 0 for b in BUTTONS},
        'ReactiveLEDs': {'leds': [dict(action=a, modeUp=0, modeDown=4,
                                    brightness=255) for a in (59, 60, 66)]},
        'ExpansionPins': {'pins': {'pcf8575': []}},
        'HETriggerOptions': {'triggers': [dict(action=-10, idle=100, active=2000,
                                             max=4095, polarity=0) for _ in range(32)]},
        'DisplayOptions': dict(enabled=0, flipDisplay=0, invertDisplay=0,
                               buttonLayout=0, buttonLayoutRight=0,
                               splashMode=0, splashChoice=0, splashDuration=0,
                               displaySaverTimeout=0, displaySaverMode=0,
                               buttonLayoutCustomOptions={
                                   key: dict(layout=0, startX=8, startY=28,
                                             buttonRadius=8, buttonPadding=2)
                                   for key in ('params', 'paramsRight')}),
        'SplashImage': {'splashImage': [0] * 1024},
        'LedOptions': dict(brightnessMaximum=255, brightnessSteps=5, dataPin=-1,
                           ledFormat=0, ledLayout=0, ledsPerButton=1,
                           ledButtonMap={b: None for b in BUTTONS}, pledType=0,
                           pledColor=0, pledPin1=-1, pledPin2=-1, pledPin3=-1,
                           pledPin4=-1, caseRGBType=0, caseRGBIndex=-1,
                           caseRGBCount=0, turnOffWhenSuspended=0),
        'CustomTheme': {'enabled': False, **{b: {'u': 0, 'd': 0} for b in BUTTONS}},
        'WiiControls': {},
    }


def merge_options(target, patch):
    for key, value in patch.items():
        if isinstance(value, dict) and isinstance(target.get(key), dict):
            merge_options(target[key], value)
        else:
            target[key] = copy.deepcopy(value)


class PreviewServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, host, port, workspace, state_file=None, simulate=False, analog_preview_dll=None):
        host = loopback_host(host)
        self.address_family = socket.AF_INET6 if ':' in host else socket.AF_INET
        self.build = Path(workspace).resolve() / 'www' / 'build'
        if not (self.build / 'index.html').is_file():
            raise ValueError('www/build/index.html missing; build the existing Web UI first')
        self.state_file = Path(state_file) if state_file else (
            Path(workspace).resolve() / '.cache' / 'psx-web-preview' / 'state.json')
        self.lock = threading.RLock()
        self.state = defaults()
        if self.state_file.exists():
            saved = json.loads(self.state_file.read_text(encoding='utf-8'))
            if not isinstance(saved, dict) or set(saved) - set(self.state):
                raise ValueError('invalid preview state file')
            for key, value in saved.items():
                if not isinstance(value, dict):
                    raise ValueError('invalid preview state options')
                merge_options(self.state[key], value)
        self.simulate = simulate
        self.analog_dll_path = Path(analog_preview_dll).resolve() if analog_preview_dll else (
            Path(workspace).resolve() / 'pc-profile-editor' / 'build' / 'Release' / 'analog_preview.dll')
        if not analog_preview_dll and not self.analog_dll_path.is_file():
            self.analog_dll_path = Path(workspace).resolve() / 'pc-profile-editor' / 'bin' / 'analog_preview.dll'
        self.analog_dll = None
        self.analog_sessions = {}
        self.held_pins = []
        super().__init__((host, port), PreviewHandler)

    def analog_model(self):
        if self.analog_dll is None:
            sizes = [ctypes.sizeof(cls) for cls in (AnalogSettings, AnalogInput, AnalogState, AnalogOutput)]
            offsets = [AnalogSettings.jogconDeadzone.offset, AnalogSettings.jogconMax.offset,
                       AnalogSettings.jogconForcebackEnabled.offset, AnalogInput.jogPosition.offset,
                       AnalogInput.jogcon.offset, AnalogState.output.offset, AnalogState.lastJogcon.offset,
                       AnalogSettings.pressureInner.offset, AnalogSettings.pressureOuter.offset]
            if sizes != [104, 24, 48, 12] or offsets != [92, 96, 100, 16, 20, 32, 44, 68, 80]:
                raise OSError('analog ABI v3 layout mismatch')
            library = ctypes.CDLL(str(self.analog_dll_path))
            library.analog_preview_version.argtypes = []
            library.analog_preview_version.restype = ctypes.c_uint32
            if library.analog_preview_version() != 3:
                raise OSError('analog DLL ABI version mismatch')
            library.analog_preview_reset.argtypes = [ctypes.POINTER(AnalogState)]
            library.analog_preview_reset.restype = None
            library.analog_preview_step.argtypes = [ctypes.POINTER(AnalogInput), ctypes.POINTER(AnalogSettings),
                ctypes.c_uint16, ctypes.c_uint8, ctypes.POINTER(AnalogState), ctypes.POINTER(AnalogOutput)]
            library.analog_preview_step.restype = None
            library.analog_preview_get_output.argtypes = [ctypes.POINTER(AnalogState), ctypes.POINTER(AnalogOutput)]
            library.analog_preview_get_output.restype = None
            if ctypes.sizeof(JogSettings) != 12 or ctypes.sizeof(JogState) != 24 or JogState.motorCommand.offset != 16:
                raise OSError('JOGCON ABI v1 layout mismatch')
            library.jogcon_model_version.argtypes = []
            library.jogcon_model_version.restype = ctypes.c_uint32
            if library.jogcon_model_version() != 1:
                raise OSError('JOGCON DLL ABI version mismatch')
            library.jogcon_model_reset.argtypes = [ctypes.POINTER(JogState), ctypes.c_uint32]
            library.jogcon_model_reset.restype = None
            library.jogcon_model_step.argtypes = [ctypes.POINTER(JogState), ctypes.c_int32,
                ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.POINTER(JogSettings), ctypes.c_uint32]
            library.jogcon_model_step.restype = None
            library.jogcon_model_guard.argtypes = [ctypes.POINTER(JogState), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint32]
            library.jogcon_model_guard.restype = None
            self.analog_dll = library
        return self.analog_dll

    def analog_session_state(self, session):
        now = time.monotonic()
        for key in list(self.analog_sessions):
            if now - self.analog_sessions[key]['access'] > 300:
                del self.analog_sessions[key]
        if session not in self.analog_sessions:
            if len(self.analog_sessions) >= 16:
                raise ValueError('too many monitor sessions')
            self.analog_sessions[session] = {
                'access': now, 'lastTick': now, 'savedTick': now, 'draftTick': now, 'revision': 0, 'options': None,
                'savedState': AnalogState(), 'draftState': AnalogState(), 'savedOptions': None, 'draftInitialized': False, 'midpoint': None,
                'input': dict(controllerId=0x79, lx=128, ly=128, rx=128, ry=128, i=0, ii=0, l=0, jogPosition=0, jogStatus=0),
                'connected': self.simulate, 'ageMs': 0, 'generation': 1, 'frameSequence': 0,
                'jogState': JogState(), 'jogClock': int(now * 1000) & 0xffffffff, 'jogMotionRemainder': 0,
                'lastJogButton': now,
            }
        state = self.analog_sessions[session]
        state['access'] = now
        return state

    def analog_monitor(self, session):
        state = self.analog_session_state(session)
        library = self.analog_model()
        now = time.monotonic()
        ticks = min(250, max(0, int((now - state['lastTick']) * 1000)))
        state['lastTick'] = now if ticks == 250 else state['lastTick'] + ticks / 1000
        state['frameSequence'] += ticks
        raw = state['input']
        connected = bool(self.simulate and state['connected'])
        identifier = raw['controllerId']
        analog = identifier in (0x53, 0x73, 0x79)
        negcon = identifier == 0x23
        jog = identifier == 0xe3
        data = AnalogInput(state['generation'], raw['lx'], raw['ly'], raw['rx'], raw['ry'],
                           raw['i'], raw['ii'], raw['l'], connected, analog, negcon, identifier, 0,
                           raw['jogPosition'], jog, raw['jogStatus'], connected and jog and state['ageMs'] < 100, 0)
        saved = {key: self.state['AddonsOptions'][key] for key in ANALOG_RANGES}
        sleep_suspected = connected and jog and now - state['lastJogButton'] >= 60
        jog_settings = JogSettings(int(saved['jogconDeadzone']), int(saved['jogconMax']), int(saved['jogconForcebackEnabled']))
        samples = []
        for _ in range(ticks):
            state['jogClock'] = (state['jogClock'] + 1) & 0xffffffff
            motor = state['jogState']
            # Only a reset return drives the PC dial. Forceback itself leaves user RAW input unchanged.
            if connected and jog and state['ageMs'] < 100 and motor.resetStatus == 2 and motor.motorPower:
                state['jogMotionRemainder'] += 400
                movement, state['jogMotionRemainder'] = divmod(state['jogMotionRemainder'], 1000)
                direction = -1 if motor.motorCommand & 0x20 else 1
                raw['jogPosition'] += direction * min(abs(raw['jogPosition']), movement)
            if connected and jog and state['ageMs'] < 100:
                library.jogcon_model_step(ctypes.byref(motor), raw['jogPosition'], raw['jogStatus'], 1, 1,
                                         ctypes.byref(jog_settings), state['jogClock'])
            else:
                library.jogcon_model_guard(ctypes.byref(motor), connected and jog, 0, state['jogClock'])
            if sleep_suspected:
                motor.motorCommand = motor.motorPower = 0
            samples.append(raw['jogPosition'])
        data.jogPosition = raw['jogPosition']
        input_mode = self.state['GamepadOptions']['inputMode']
        midpoint = 0x8000 if input_mode in (1, 3, 4, 13, 14, 16) else 0x7fff
        if state['midpoint'] != midpoint:
            state['draftState'] = AnalogState()
            state['draftInitialized'] = False
            state['draftTick'] = now
        saved_changed = state['savedOptions'] != saved or state['midpoint'] != midpoint
        if saved_changed:
            library.analog_preview_reset(ctypes.byref(state['savedState']))
            state['savedOptions'] = copy.deepcopy(saved)
            state['savedTick'] = now
        state['midpoint'] = midpoint
        computed = []
        for options, filter_state, clock in ((saved, state['savedState'], 'savedTick'), (state['options'], state['draftState'], 'draftTick')):
            if options is None:
                computed.append(None)
                continue
            settings = dll_settings(options)
            output = AnalogOutput()
            if clock == 'draftTick' and not state['draftInitialized']:
                library.analog_preview_reset(ctypes.byref(filter_state))
                state['draftInitialized'] = True
            model_ticks = min(250, max(0, int((now - state[clock]) * 1000)))
            # 設定変更後の最初のGETでも有効な中央値と軸マスクを返す。
            if clock == 'savedTick' and saved_changed:
                model_ticks = max(1, model_ticks)
            state[clock] = now if model_ticks == 250 else state[clock] + model_ticks / 1000
            for tick in range(model_ticks):
                data.jogPosition = samples[min(len(samples) - 1, max(0, len(samples) - model_ticks + tick))] if samples else raw['jogPosition']
                library.analog_preview_step(ctypes.byref(data), ctypes.byref(settings), midpoint, 1,
                                           ctypes.byref(filter_state), ctypes.byref(output))
            library.analog_preview_get_output(ctypes.byref(filter_state), ctypes.byref(output))
            values = {key: getattr(output, key) if connected and output.validityMask & (1 << index) else None
                      for index, key in enumerate(('lx', 'ly', 'rx', 'ry', 'lt', 'rt'))}
            values.update(validityMask=output.validityMask, reset=bool(output.reset))
            computed.append(values)
        measured = {key: raw[key] if connected and ((analog and key in ('lx', 'ly', 'rx', 'ry')) or
                    (negcon and key in ('lx', 'i', 'ii', 'l'))) else None for key in ('lx', 'ly', 'rx', 'ry', 'i', 'ii', 'l')}
        measured.update(jogPosition=raw['jogPosition'] if connected and jog else None,
                        jogStatus=raw['jogStatus'] if connected and jog else None)
        motor = state['jogState']
        return dict(raw=measured, savedComputed=computed[0], draftComputed=computed[1], simulation=self.simulate,
                    jogcon=dict(position=measured['jogPosition'], status=measured['jogStatus'], motorPower=motor.motorPower,
                                motorCommand=motor.motorCommand, ready=bool(motor.ready), resetState=motor.resetStatus,
                                sleepSuspected=bool(sleep_suspected)) if connected and jog else None,
                    draftErrors=[], metadata=dict(connected=connected, controllerId=identifier if connected else None,
                    ageMs=state['ageMs'] if connected else None, frameSequence=state['frameSequence'],
                    timestampUs=int(now * 1000000), generation=state['generation'], midpoint=midpoint,
                    inputMode=self.state['GamepadOptions']['inputMode'], filterUpdatePolicy='fixed_1ms_preview; runtime_per_loop_legacy',
                    previewTickUs=1000, previewMaxCatchUpTicks=250, computedKind='calculated_not_usb_report',
                    runtimeFilterUpdatePolicy='per_loop_legacy', calculatedNotUsb=True,
                    jogconOutputMode=saved['analogAdc1Mode'], jogconMotionCountsPerSecond=400,
                    version=2, jogcon=bool(connected and jog), jogReady=bool(motor.ready), jogStatus=measured['jogStatus'], jogResetState=motor.resetStatus,
                    rawValidityMask=sum(1 << index for index, key in enumerate(('lx', 'ly', 'rx', 'ry', 'i', 'ii', 'l')) if measured[key] is not None),
                    draftRevision=state['revision'], draftSession=session, hardwareAccess=False))

    def save(self, candidate):
        self.state_file.parent.mkdir(parents=True, exist_ok=True)
        # Atomic replacement keeps concurrent saves and interrupted writes safe.
        name = None
        try:
            with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8',
                                             dir=self.state_file.parent,
                                             delete=False) as stream:
                name = stream.name
                json.dump(candidate, stream, indent=2, allow_nan=False)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(name, self.state_file)
            self.state = candidate
        finally:
            if name and os.path.exists(name):
                os.unlink(name)

    def banner(self):
        mode = 'SIMULATION enabled (not real input)' if self.simulate else 'No controller connected'
        banner = ('<aside id="psx-preview-banner" role="status" style="position:relative;pointer-events:none;'
                  'z-index:1;background:#ffdf80;color:#201800;padding:12px;'
                  'font:600 14px system-ui;overflow-wrap:anywhere">'
                  'PSX WEB PREVIEW / PC only / ' + html.escape(mode) +
                  '. Saves, backup imports, reset and reboot affect local preview only. '
                  'No RNDIS, COM, SWD or hardware access.</aside>')
        if self.simulate:
            banner += '<details id="psx-preview-simulation" style="padding:8px;background:#fff3cd;color:#201800"><summary>PSXボタン模擬入力（割り当て取得のPCテスト用）</summary><p>チェックするとPSXボタンを押した状態になります。次の割り当てへ進む前にチェックを外してください。アナログ入力は「設定値確認」のスライダーを使用します。</p>'
            for slot, label in enumerate(PSX_BUTTONS, 8):
                banner += f'<label style="display:inline-block;margin:4px"><input type="checkbox" value="{slot}">{label}</label>'
            banner += '</details><script src="/preview-simulation.js"></script>'
        return banner


class PreviewHandler(BaseHTTPRequestHandler):
    def respond(self, status, data, content_type='application/json; charset=utf-8'):
        if not isinstance(data, bytes):
            data = json.dumps(data, allow_nan=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-PSX-Web-Preview', 'local-only; no-hardware')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(data)

    def do_HEAD(self):
        self.do_GET()

    def allowed_request(self):
        host = self.headers.get('Host', '')
        port = self.server.server_port
        allowed = {f'127.0.0.1:{port}', f'localhost:{port}',
                   f'[{self.server.server_address[0]}]:{port}',
                   f'{self.server.server_address[0]}:{port}'}
        if host not in allowed:
            self.respond(403, {'error': 'loopback Host required'})
            return False
        origin = self.headers.get('Origin')
        if origin and origin not in {f'http://{h}' for h in allowed}:
            self.respond(403, {'error': 'same-origin requests only'})
            return False
        return True

    def do_GET(self):
        if not self.allowed_request():
            return
        path = unquote(urlsplit(self.path).path)
        if path.startswith('/api/'):
            self.api(path[5:])
            return
        if path == '/preview-simulation.js' and self.server.simulate:
            script = """document.getElementById('psx-preview-simulation').addEventListener('change', async () => {
const heldPins = Array.from(document.querySelectorAll('#psx-preview-simulation input:checked'), e => Number(e.value));
await fetch('/api/previewSimulate', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({heldPins})});
});"""
            self.respond(200, script.encode(), 'text/javascript; charset=utf-8')
            return
        try:
            if '\\' in path or '\x00' in path or '..' in path.split('/'):
                raise ValueError('invalid path')
            file = (self.server.build / path.lstrip('/')).resolve()
            file.relative_to(self.server.build.resolve())
            if (path.rstrip('/') or '/') in SPA_ROUTES or path == '/index.html':
                file = self.server.build / 'index.html'
            if not file.is_file():
                self.respond(404, {'error': 'not found'})
                return
            data = file.read_bytes()
            mime = mimetypes.guess_type(str(file))[0] or 'application/octet-stream'
            if file.name == 'index.html':
                data = re.sub(rb'(<body\b[^>]*>)',
                              lambda m: m[0] + self.server.banner().encode(),
                              data, count=1, flags=re.IGNORECASE)
            self.respond(200, data, mime)
        except ValueError:
            self.respond(403, {'error': 'path outside build is forbidden'})
        except OSError:
            self.respond(500, {'error': 'cannot read preview asset'})

    def do_POST(self):
        if not self.allowed_request():
            return
        path = urlsplit(self.path).path
        if not path.startswith('/api/'):
            self.respond(404, {'error': 'not found'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if length <= 0 or length > MAX_BODY or self.headers.get('Transfer-Encoding'):
                raise ValueError('invalid request size')
            self.connection.settimeout(5)
            body = self.rfile.read(length)
            if len(body) != length:
                raise ValueError('incomplete request')
            payload = json.loads(body, parse_constant=lambda _: (_ for _ in ()).throw(ValueError('non-finite JSON')))
            if not isinstance(payload, dict):
                raise ValueError('JSON object required')
        except (ValueError, UnicodeError, OSError):
            self.respond(400, {'error': 'valid bounded JSON object required'})
            return
        self.api(path[5:], payload)

    def api(self, endpoint, payload=None):
        server = self.server
        with server.lock:
            if payload is None:
                if endpoint == 'resetJogconDial':
                    self.respond(405, {'error': 'POST required', 'code': 'METHOD_NOT_ALLOWED'})
                    return
                if endpoint == 'getAnalogMonitor':
                    try:
                        query = parse_qs(urlsplit(self.path).query)
                        requested = query.get('session', [self.headers.get('X-Analog-Session', '')])[0]
                        if not requested:
                            active = [key for key, value in server.analog_sessions.items() if value['connected'] and
                                      value['input']['controllerId'] == 0xe3 and time.monotonic() - value['access'] <= 300]
                            requested = max(active, key=lambda key: server.analog_sessions[key]['access']) if active else '00000000-0000-0000-0000-000000000000'
                        session = analog_session({'session': requested})
                        data = server.analog_monitor(session)
                    except (OSError, AttributeError):
                        self.respond(503, {'error': 'analog_preview.dll unavailable: build the shared C++ model',
                                           'code': 'ANALOG_MODEL_UNAVAILABLE', 'simulation': server.simulate})
                        return
                    except ValueError as error:
                        self.respond(400, {'error': str(error), 'code': 'ANALOG_MONITOR_INVALID'})
                        return
                elif endpoint == 'getFirmwareVersion':
                    data = dict(version='0.7.12-preview', boardConfig='PSX Preview',
                                boardConfigLabel='PSX PC Preview (NO HARDWARE)',
                                boardConfigFileName='preview-not-firmware',
                                architecture='PC', buildType='preview', build='local')
                elif endpoint == 'getMemoryReport':
                    data = dict(totalFlash=1, usedFlash=0, physicalFlash=0,
                                staticAllocs=0, totalHeap=1, usedHeap=0)
                elif endpoint == 'getUsedPins':
                    data = {'usedPins': [0, 1, 2, 3, 4, 6, 7, 26, 27, 28]}
                elif endpoint == 'getHeldPins':
                    data = {'heldPins': list(server.held_pins) if server.simulate else [],
                            'connected': bool(server.simulate), 'simulated': server.simulate}
                elif endpoint == 'abortGetHeldPins':
                    data = {'success': True}
                elif endpoint == 'resetSettings':
                    if self.command == 'HEAD':
                        self.respond(405, {'error': 'reset requires GET'})
                        return
                    try:
                        server.save(defaults())
                        server.held_pins = []
                    except OSError:
                        self.respond(500, {'error': 'cannot save local preview state'})
                        return
                    data = {'success': True}
                elif endpoint.startswith('get') and endpoint[3:] in server.state:
                    data = copy.deepcopy(server.state[endpoint[3:]])
                    if endpoint == 'getGamepadOptions':
                        # 実機GETは16進文字列、POSTは整数。往復でUI検証を壊さない。
                        for key in ('usbVendorID', 'usbProductID'):
                            if type(data.get(key)) is int:
                                data[key] = f'{data[key]:04x}'
                else:
                    self.respond(404, {'error': 'unsupported preview API'})
                    return
                self.respond(200, {**data, 'preview': True})
                return
            if endpoint == 'resetJogconDial':
                try:
                    if set(payload) - {'session'}:
                        raise ValueError('session only')
                    requested = analog_session(payload) if 'session' in payload else None
                    now = time.monotonic()
                    sessions = [requested] if requested else list(server.analog_sessions)
                    targets = [key for key in sessions if key in server.analog_sessions and server.simulate and
                               server.analog_sessions[key]['connected'] and server.analog_sessions[key]['input']['controllerId'] == 0xe3 and
                               server.analog_sessions[key]['ageMs'] < 100 and now - server.analog_sessions[key]['access'] <= 300]
                    if not targets:
                        self.respond(409, {'error': 'JOGCON not connected or ready', 'code': 'JOGCON_NOT_READY'})
                        return
                    library = server.analog_model()
                    for key in targets:
                        state = server.analog_sessions[key]
                        if time.monotonic() - state['lastJogButton'] >= 60:
                            self.respond(409, {'error': 'jogcon_sleep_suspected_press_physical_button'})
                            return
                        library.jogcon_model_reset(ctypes.byref(state['jogState']), state['jogClock'])
                        state['jogMotionRemainder'] = 0
                    self.respond(200, {'success': True, 'resetState': 2, 'sessionsReset': len(targets),
                                       'preview': True, 'hardwareAccess': False})
                    return
                except (OSError, AttributeError):
                    self.respond(503, {'error': 'shared JOGCON model unavailable', 'code': 'ANALOG_MODEL_UNAVAILABLE'})
                    return
                except ValueError as error:
                    self.respond(400, {'error': str(error), 'code': 'INVALID_JOGCON_RESET'})
                    return
            elif endpoint == 'setAnalogPreviewDraft':
                try:
                    if set(payload) != {'session', 'revision', 'options'}:
                        raise ValueError('session, revision and options only')
                    session = analog_session(payload)
                    revision = payload.get('revision')
                    if type(revision) is not int or not 1 <= revision <= 0xffffffff:
                        raise ValueError('positive uint32 revision required')
                    validate_analog_options(payload.get('options'))
                    state = server.analog_session_state(session)
                    if revision <= state['revision']:
                        self.respond(409, {'error': 'stale draft revision', 'code': 'STALE_DRAFT_REVISION'})
                        return
                    if state['options'] != payload['options']:
                        state['draftState'] = AnalogState()
                        state['draftInitialized'] = False
                        state['draftTick'] = time.monotonic()
                    state['options'] = copy.deepcopy(payload['options'])
                    state['revision'] = revision
                except ValueError as error:
                    self.respond(400, {'error': str(error), 'code': 'INVALID_ANALOG_DRAFT'})
                    return
            elif endpoint == 'setAnalogSimulationInput':
                if not server.simulate:
                    self.respond(403, {'error': 'simulation is disabled', 'code': 'SIMULATION_DISABLED'})
                    return
                try:
                    allowed = {'session', 'controllerId', 'lx', 'ly', 'rx', 'ry', 'i', 'ii', 'l', 'connected', 'ageMs', 'jogPosition', 'jogStatus'}
                    if set(payload) - allowed:
                        raise ValueError('unknown simulation field')
                    state = server.analog_session_state(analog_session(payload))
                    candidate = {**state['input'], **{key: value for key, value in payload.items() if key in state['input']}}
                    if any(type(value) is not int or not 0 <= value <= 255 for key, value in candidate.items() if key != 'jogPosition'):
                        raise ValueError('raw simulation values must be integers 0..255')
                    if type(candidate['jogPosition']) is not int or not -32768 <= candidate['jogPosition'] <= 32767:
                        raise ValueError('jogPosition must be an integer -32768..32767 counts')
                    if candidate['controllerId'] not in (0x41, 0x53, 0x73, 0x79, 0x23, 0xe3):
                        raise ValueError('unsupported PSX controller ID')
                    connected = payload.get('connected', True)
                    age = payload.get('ageMs', 0)
                    if type(connected) is not bool or type(age) is not int or not 0 <= age <= 60000:
                        raise ValueError('connected must be bool and ageMs must be 0..60000')
                    previous_jog = state['connected'] and state['input']['controllerId'] == 0xe3
                    new_jog = connected and candidate['controllerId'] == 0xe3
                    library = server.analog_model() if previous_jog or new_jog else None
                    if new_jog and not previous_jog:
                        state['lastJogButton'] = time.monotonic()
                        # A newly attached PC dial starts at physical zero, before any subsequent slider input.
                        candidate['jogPosition'] = 0
                        state['jogState'] = JogState()
                    if previous_jog and not new_jog:
                        library.jogcon_model_guard(ctypes.byref(state['jogState']), 0, 0, state['jogClock'])
                    if candidate['controllerId'] != state['input']['controllerId'] or connected != state['connected']:
                        state['generation'] += 1
                    state['input'] = candidate
                    state['connected'] = connected
                    state['ageMs'] = age
                    if new_jog:
                        saved = server.state['AddonsOptions']
                        settings = JogSettings(int(saved['jogconDeadzone']), int(saved['jogconMax']), int(saved['jogconForcebackEnabled']))
                        if age < 100:
                            library.jogcon_model_step(ctypes.byref(state['jogState']), candidate['jogPosition'], candidate['jogStatus'], 1, 1,
                                                     ctypes.byref(settings), state['jogClock'])
                        else:
                            library.jogcon_model_guard(ctypes.byref(state['jogState']), 1, 0, state['jogClock'])
                except (OSError, AttributeError):
                    self.respond(503, {'error': 'shared JOGCON model unavailable', 'code': 'ANALOG_MODEL_UNAVAILABLE'})
                    return
                except ValueError as error:
                    self.respond(400, {'error': str(error), 'code': 'INVALID_SIMULATION_INPUT'})
                    return
            elif endpoint == 'previewSimulate' and server.simulate:
                pins = payload.get('heldPins')
                if not isinstance(pins, list) or any(type(p) is not int or not 8 <= p <= 23 for p in pins):
                    self.respond(400, {'error': 'PSX virtual slots 8..23 required'})
                    return
                server.held_pins = sorted(set(pins))
                if pins:
                    for model in server.analog_sessions.values():
                        model['lastJogButton'] = time.monotonic()
            elif endpoint == 'reboot':
                pass  # A preview reboot must never reboot the PC or any device.
            elif endpoint.startswith('set') and endpoint[3:] in server.state:
                candidate = copy.deepcopy(server.state)
                key = endpoint[3:]
                if key == 'GamepadOptions':
                    try:
                        validate_gamepad_modes(payload)
                    except ValueError as error:
                        self.respond(400, {'error': str(error)})
                        return
                if key == 'AddonsOptions':
                    try:
                        merged = {**candidate[key], **payload}
                        validate_analog_options({field: merged[field] for field in ANALOG_RANGES})
                    except ValueError as error:
                        self.respond(400, {'error': str(error)})
                        return
                if key == 'SplashImage' and isinstance(payload.get('splashImage'), str):
                    import base64
                    try:
                        payload = {'splashImage': list(base64.b64decode(payload['splashImage'], validate=True))}
                    except ValueError:
                        self.respond(400, {'error': 'invalid splash image'})
                        return
                merge_options(candidate[key], payload)
                try:
                    server.save(candidate)
                except OSError:
                    self.respond(500, {'error': 'cannot save local preview state'})
                    return
            elif endpoint == 'setPreviewDisplayOptions':
                pass
            else:
                self.respond(404, {'error': 'unsupported preview API'})
                return
            response = {'success': True, 'preview': True, 'hardwareAccess': False}
            if endpoint == 'setAnalogPreviewDraft':
                response.update(draftSession=session, draftRevision=revision)
            self.respond(200, response)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', type=loopback_host, default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--workspace', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--state-file', type=Path)
    parser.add_argument('--simulate', action='store_true', help='enable clearly labeled debug PSX input toolbar')
    parser.add_argument('--analog-preview-dll', type=Path, help='explicit path to the compiled shared analog_preview.dll')
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error('port must be 1..65535')
    try:
        server = PreviewServer(args.host, args.port, args.workspace, args.state_file, args.simulate, args.analog_preview_dll)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    host = f'[{args.host}]' if ':' in args.host else args.host
    print(f'PSX preview ready: http://{host}:{server.server_port}/ (NO HARDWARE)', flush=True)
    print(f'Local state: {server.state_file}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
