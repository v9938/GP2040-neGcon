# GP2040 PSX Preset TXT v3

## 目的と操作

PC版オフライン編集アプリとWeb設定画面の共通形式です。ファイル名は `*.gp2040-psx.txt`。JSON文書ではなく、人が読めるINI形式です。選択した1プロファイルのPSX 16入力と、8種類の共有ADDON（PSXアナログ、Keyboard/Mouse Host、Gamepad USB Host、基板LED、Reactive LED、BOOTSEL、Input Reverse、Input Macro）を含みます。

Webの「PCへ保存（TXT）」は編集中のプロファイルを出力します。共有設定はRAM下書きがあればそれを、なければGETした機器設定を出力します。「PCから読込（TXT）」は全項目の検証後、確認ダイアログで選択中のプロファイルだけに反映します。`source`は出力元の記録であり読込先や現在使用中のプロファイルを切り替えません。他のプロファイルは保持します。

共有設定を読み込むチェックボックスは初期値ONです。共有設定は全プロファイルに影響し、未保存の共有下書きを置き換えます。読込もPC出力も機器への書込を行いません。機器への保存は「読込設定を保存」で選択プロファイルと未保存の共有読込カテゴリを順次保存します。または各画面の保存ボタンでカテゴリ別に保存できます。通常のプロファイル保存は共有設定を保存しません。RAM下書きは画面間で保持しますがブラウザー再読込で消えます。異なるAPIカテゴリの保存はトランザクションではなく、失敗時は一部が保存済みの場合があります。自動再起動は行いません。

USB出力モード、USB VID/PID、認証鍵、固定GPIO、Flash全体、移行情報、未使用ADDONは含みません。機器全体のバックアップ形式ではありません。

## 文法

- UTF-8、最大65536バイト（BOMを含む）。先頭UTF-8 BOMは許可。改行はLFまたはCRLF。単独CRは不可。
- セクションとキーは大文字小文字を区別。前後のASCIIスペース・タブを許可。空行と、先頭の空白を除いた最初の文字が`#`の行だけをコメントとする。行末コメントは不可。引用文字列内の`#`は文字。
- セクション行は`[Name]`。代入は`key = value`。セクション名は`[A-Za-z][A-Za-z0-9.]*`、キーは`[A-Za-z][A-Za-z0-9_.]*`。
- 値は引用文字列、`true`/`false`、十進数`-?(0|[1-9][0-9]*)(\.[0-9]+)?`、または非負の16進整数`0x[0-9a-fA-F]+`。指数、NaN、Infinity、先頭`+`、先頭ゼロ、配列、埋込JSONオブジェクトは禁止。シリアライザは十進数を出力する。
- 引用文字列だけJSON string grammarを使用する。`\"`、`\\`、`\/`、`\b`、`\f`、`\n`、`\r`、`\t`、`\uXXXX`を構文上認識するが、復号後の制御文字U+0000..001F/U+007Fと不正サロゲートは拒否する。日本語はMacroラベルで使用可能。
- 以下のセクションとキーは全て必須。重複セクション、重複キー、未知セクション、未知キー、欠落を全てエラーとして拒否する。フィールドの黙示削除や未知情報の保持は行わない。出力は常にv3。読込はv1/v2/v3だけを許可し、0・4など他のバージョンは拒否する。
- v1はJOGCONの3項目とneGcon感圧の新規9項目を含まない旧スキーマと完全一致する必要がある。既存の全項目を検証後、`jogconDeadzone = 0`、`jogconMax = 100`、`jogconForcebackEnabled = 1`と新規9項目の初期値を補完する。v2は既存のJOGCON3項目も必須で、新規9項目だけを補完する。各旧版で新しいキーを指定しても未知キーとして拒否する。v3は9項目とも必須で欠落を補完しない。マッピング・マクロ・既存設定は保持し、再出力はv3となる。
- **旧TXTの動作変更に注意:** 旧版も新しい加算方式で動作する。旧fullの軸全範囲への上書きという意味は廃止される。移行は既存のmodeを変更せず、14=RT / 13=LTを新規初期値12=RY center- / 11=RY center+へ置換しない。既存ファイル読込時に新規初期値で上書きしない。旧版にもneGconの感圧同士・twistとの重複禁止を適用しない。DSの同一ペア出力禁止は全バージョンで維持する。

## 固定セクション

| Section | Key | 型・制約 |
|---|---|---|
| Preset | format | `"GP2040-PSX-PRESET"` |
| Preset | version | 整数3（読込のみ旧スキーマの整数1/2も許可） |
| Profile | label | 引用文字列、ASCII英数字とスペースだけ、0..16バイト |
| Profile | enabled | bool |
| Profile | source | 整数1..6、出力元情報だけ |

基本または使用中のプロファイルに`enabled = false`を読み込む操作はWebで拒否します。

## Mapping / MappingMasks

`[Mapping]`は次の16キーを全て1回ずつ持ち、値は`proto/enums.proto`の`GpioAction`名の引用文字列です。`RESERVED`と`ASSIGNED_TO_ADDON`は不可。それ以外の定義済み名を使用します。数値actionは不可。

| 論理キー | Web/API仮想スロット | 意味 |
|---|---|---|
| UP | pin08 | 十字上 |
| DOWN | pin09 | 十字下 |
| RIGHT | pin10 | 十字右 |
| LEFT | pin11 | 十字左 |
| CROSS | pin12 | CROSS / neGcon I |
| CIRCLE | pin13 | CIRCLE / neGcon A |
| R2 | pin14 | R2 |
| L2 | pin15 | L2 |
| SQUARE | pin16 | SQUARE / neGcon II |
| TRIANGLE | pin17 | TRIANGLE / neGcon B |
| R1 | pin18 | R1 / neGcon R |
| L1 | pin19 | L1 / neGcon L |
| SELECT | pin20 | SELECT |
| START | pin21 | START |
| L3 | pin22 | 左スティック押下 |
| R3 | pin23 | 右スティック押下 |

`[MappingMasks]`は各論理キーに`KEY.buttons`（整数0..65535）と`KEY.dpad`（整数0..15）の32項目。`CUSTOM_BUTTON_COMBO`以外のactionでは両方0が必須。buttonsはbit0..15がB1/B2/B3/B4/L1/R1/L2/R2/S1/S2/L3/R3/A1/A2/A3/A4。dpadはbit0..3がUP/DOWN/LEFT/RIGHTです。

## Addons

`[Addons]`は下表の全項目を必須とする数値だけのセクションです。0/1フラグもboolではなく整数です。`AnalogInputEnabled`は固定ONなのでファイルには含めません。

| Key | 制約 |
|---|---|
| analogAdc1Mode / analogAdc2Mode | 整数1=LS、2=RS、2つは異なる出力 |
| analogAdc1Invert / analogAdc2Invert | 整数0=なし、1=X、2=Y、3=XY |
| negconIMode / negconIIMode / negconLMode | 整数0..14（下記） |
| negconIThreshold / negconIIThreshold / negconLThreshold | 整数0..255 |
| negconIDigitalEnabled / negconIIDigitalEnabled / negconLDigitalEnabled | 整数0/1、初期値1（APIのboolも0/1で表現、TXT/nativeはbool不可） |
| negconIInnerDeadzone / negconIIInnerDeadzone / negconLInnerDeadzone | 符号なし整数0..100（%）、初期値0 |
| negconIOuterDeadzone / negconIIOuterDeadzone / negconLOuterDeadzone | 符号なし整数0..100（%）、初期値100、同じI/II/Lのinnerより大きい |
| jogconDeadzone | 非負整数0..32766、jogconMax未満、初期値0 |
| jogconMax | 整数1..32767、初期値100 |
| jogconForcebackEnabled | 整数0/1、初期値1（bool不可） |
| forced_circularity / forced_circularity2 | 整数0/1 |
| inner_deadzone / inner_deadzone2 | 整数0..100（%） |
| outer_deadzone / outer_deadzone2 | 整数0..100（%）、同じ番号のinnerより大きい |
| auto_calibrate / auto_calibrate2 | 整数0/1 |
| analog_smoothing / analog_smoothing2 | 整数0/1 |
| smoothing_factor / smoothing_factor2 | 有限十進数0..100 |
| analog_error / analog_error2 | 整数1000,990,979,969,958,946,934,922,911,900,890,876,863,848,834,821のいずれか |
| BootselButtonAddonEnabled | 整数0/1 |
| bootselButtonMap | 0または通常ボタン/dpadの単一bitマスク（bit0..19） |
| BoardLedAddonEnabled | 整数0/1 |
| onBoardLedMode | 整数0..3（OFF、Mode Indicator、Input Test、PS認証表示） |
| ReverseInputEnabled | 整数0/1 |
| reverseActionUp / reverseActionDown / reverseActionLeft / reverseActionRight | 整数0=無視、1=反転、2=ニュートラル |
| KeyboardHostAddonEnabled | 整数0/1 |
| GamepadUSBHostAddonEnabled | 整数0/1 |
| keyboardHostMouseLeft / keyboardHostMouseMiddle / keyboardHostMouseRight | 0またはuint32の単一bitマスク |
| keyboardHostMouseSensitivity | 有限十進数0..100 |
| keyboardHostMouseMovement | 整数0=なし、1=LS、2=RS |
| ReactiveLEDAddonEnabled | 整数0/1 |
| InputMacroAddonEnabled | 整数0/1、Macro本体は別セクション |

neGcon modeは0=なし、1=LX full、2=LX center+、3=LX center-、4=LY full、5=LY center+、6=LY center-、7=RX full、8=RX center+、9=RX center-、10=RY full、11=RY center+、12=RY center-、13=LT、14=RT。0..14の既存ドロップダウンを維持し、任意の出力先を選べる。新規初期値はI=12（RY center-）、II=11（RY center+）、L=0。

I/II/Lの感圧出力は**現在の出力値への加算**であり軸の占有・上書きではない。inner以下を0、outer以上を最大として感圧を補正済み押込み`u`（0..1）に正規化する。fullは`(u-0.5)*65535`を既存軸へ加算し、0で負の半スケール（-32767.5）、最大の1で正の半スケール（+32767.5）となり、軸の最小から最大への絶対値変換ではない。fullは未押下でも既存値を負方向へ動かす点に注意する。center+/-は`u*(+/-32767.5)`を加算する。LT/RTは`u*255`を加算する。同じ軸・トリガーのI/II/L同士、およびtwistと感圧の重複を許可し、全寄与を合計した最後だけ軸0..65535／トリガー0..255にclampする。途中でclampして相反する寄与を失わない。DSとneGconの入力方式は別であり、DSの2ペアは引き続き異なるLS/RS出力を選ぶ。

各I/II/Lセクションはinner/outerの2つの%入力とDigitalEnabledスイッチを持つ。DigitalEnabled=0はデジタル押下だけを無効にし、感圧アナログ出力は維持する。閾値入力は無効表示にするが保存値は保持し、再有効化で復元する。無効時も閾値0..255とinner<outerを検証する。新規9項目はアナログ設定stateとpreviewに含め、Web数値バリデーションは`PRESET_ADDON_RULES`を中心に共有する。ネイティブ汎用GUIでも9項目は`Kind::Number`で、0/1フラグに`Kind::Boolean`を使わない。

JOGCONの3項目は保存するユーザー設定です。PC模擬モーターの一時状態や実機モーター初期化の実験情報はTXTに含めません。TXTバージョンはアナログDLLのABIバージョンとは独立しています。

## KeyboardHostMap

`[KeyboardHostMap]`は次の33キーを全て持つ整数0..255（USB HIDキーコード、0=未割当）のセクション。`pin`と`error`は含めません。

`Up Down Left Right B1 B2 B3 B4 L1 R1 L2 R2 S1 S2 L3 R3 A1 A2 A3 A4 E1 E2 E3 E4 E5 E6 E7 E8 E9 E10 E11 E12 Fn`

## Reactive LED

`[Reactive.0]`=赤、`[Reactive.1]`=緑、`[Reactive.2]`=青。GPIOは含めません。各セクションは4項目。

| Key | 制約 |
|---|---|
| action | 引用GpioAction名、NONE、BUTTON_PRESS_UP..BUTTON_PRESS_R3（値1..18）、ANALOG_DIRECTION_LS_X_NEG..ANALOG_DIRECTION_RS_Y_POS（値59..66）のいずれか |
| modeUp | 整数0..4 |
| modeDown | 整数0..4 |
| brightness | 整数0..255 |

modeは0=OFF、1=ON、2=fade-in、3=fade-out、4=アナログ比例。初期actionは赤59、緑60、青66、modeUp=0/modeDown=4/brightness=255です。

## Macro

`[Macro.0]`..`[Macro.5]`は無効Macroも含め全6件必須。各セクションは以下の9項目。

| Key | 型・制約 |
|---|---|
| macroLabel | 引用文字列、0..64 UTF-8バイト、制御文字不可 |
| macroType | 整数1=Press、2=Hold Repeat、3=Toggle |
| enabled | 整数0/1 |
| exclusive | 整数0/1 |
| interruptible | 整数0/1 |
| showFrames | 整数0/1、表示用、保存時間単位は変わらない |
| useMacroTriggerButton | 整数0/1 |
| macroTriggerButton | 通常ボタン/dpadマスク0..1048575（bit0..19） |
| inputCount | 整数0..30 |

各Macroは`inputCount`件の`[Macro.i.Input.0]`..`[Macro.i.Input.N-1]`を持ちます。添字は0から連続で欠番・余分を拒否します。各入力は`buttonMask`、`duration`、`waitDuration`の3項目。buttonMaskは整数0..1048575、時間は整数0..4294967295、durationとwaitDurationの合計も4294967295以下です。時間は**マイクロ秒**（1frameのUI換算は16666μs）。maskはbit0..15が上記buttons、bit16..19がUP/DOWN/LEFT/RIGHT。ファームウェアのMacro実行側は拡張Eボタンを許可しません。有効MacroはinputCount>=1が必要で、useMacroTriggerButton=1ならmacroTriggerButton!=0が必須です。MacroトリガーGPIO・Macro基板LED制御は含みません。

## Web実装契約

`www/src/Services/PsxPreset.ts`は`PsxPreset`、`serializePreset(preset): string`、`parsePreset(text): PsxPreset`、`validatePreset(preset): PsxPreset`を公開。検証失敗はError。mappingキーはTXTと同じ論理名、actionはWeb内部数値。addons.keyboardHostMapは数値マップ。macrosは`{macroList}`、reactiveLEDsは`{leds}`です。

`www/src/Services/PsxDraft.ts`はRAMコピーとして`stagePresetShared(preset, profileIndex?)`、`peekPendingPresetShared`、`consumePendingPresetShared`、`clearPendingPresetShared`、`getPendingPresetCategories`、`getPendingPresetProfileIndex`、`getAddonDraft/setAddonDraft`、`getReactiveDraft/setReactiveDraft`、`getMacroDraft/setMacroDraft`を公開。stageは`psx-preset-staged`イベントを発行。stage後のAddon下書きはフォーム互換keyboardHostMap（`{key,pin:-1,error:null}`）。各set関数にnullを渡すとそのカテゴリだけ破棄し、pendingカテゴリもクリアします。pendingPresetは読込時スナップショット、編集中の実値はgetDraftから取得してください。複合保存成功は`psx-preset-applied`イベントで通知します。

現行v3サンプルは`example.gp2040-psx.txt`（I=12/II=11、新規9項目の初期値）、旧v1互換サンプルは`legacy-v1.gp2040-psx.txt`（既存14/13を保持）。ネイティブアプリは同一構文と制約を検証し、未知項目・不正ファイルを部分反映してはいけません。ネイティブの新規作成はv3で、汎用プロパティ一覧からJOGCONの3項目とneGconの9項目を数値として編集できます。
