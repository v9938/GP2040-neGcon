# neGcon ソースのビルド

この手順は依存ライブラリーと `lib/httpd/fsdata.c` を含むGP2040-CE `0.7.12-neGcon` の完全なソース配布を対象とします。CMakeのプロジェクトバージョンは数値の `0.7.12` のままです。ソースアーカイブは `.git` がなくてもビルドできる必要があり、配布ソースのビルドにGitは不要です。元の `README.md` とルートの `LICENSE` は変更せず、`README-neGcon.md` とともに保持してください。

## 外部ツール

- CMakeを `PATH` に配置するか、`-CMakeExecutable` で指定してください。Ninjaを推奨します。Python 3は `venv` に対応したものが必要です。
- Raspberry Pi Pico SDKは厳密に `2.1.1` を使用し、このソースツリーの外に配置してください。`PICO_SDK_PATH` または `-PicoSdkPath` で指定できます。ラッパーは `$HOME/.pico-sdk/sdk/2.1.1` も確認します。Windowsではホームディレクトリーに `$env:USERPROFILE` を使用します。
- Arm GNU Toolchainは `14_2_Rel1 / 14.2.Rel1` を使用してください。`arm-none-eabi-gcc` を `PATH` に配置するか、`SDK_TOOLCHAIN` / `PICO_TOOLCHAIN_PATH` または `-ToolchainPath` で指定します。`-ToolchainPath` はインストール先のルートまたは `bin` を受け付けます。任意のPico拡張機能のツールディレクトリーは既存の `PATH` の後ろに追加され、個人のパスは埋め込まれません。
- Pythonパッケージは `lib/nanopb/extra/requirements.txt` に従い、`protobuf>=3.19`、`grpcio-tools==1.71.0`、`setuptools<81` を使用します。互換性のある既存環境を使用できます。新しい環境ではprotobufを生成する前にこれらをインストールしてください。
- SDKが必要とするサブモジュールなどの依存ソースも事前に用意してください。picotool `2.1.1` をローカルにインストールし、`picotoolConfig.cmake` を含むディレクトリーを `-PicotoolDir` または `picotool_DIR` で指定します。ラッパーは `$HOME/.pico-sdk/picotool/2.1.1/picotool` も確認します。実行ファイルを `PATH` に配置するだけではSDKのCMakeパッケージ検索には不十分です。ラッパーはSDKによるpicotoolの自動ダウンロードへの切り替えを拒否します。

SDKは同梱していません。[公式Pico SDKリリース](https://github.com/raspberrypi/pico-sdk/releases/tag/2.1.1)から明示的に取得してください。GitHubのSDKソースZIPにはサブモジュールの内容がないため、次の再帰的なGit取得を推奨します。

```sh
git clone --branch 2.1.1 --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git pico-sdk-2.1.1
```

これは利用者が実行する依存ソースの取得であり、Releaseビルドが自動実行する処理ではありません。指定したArmリリースは[公式ツールチェーン配布](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)を参照してください。同梱の依存ライブラリーが欠けている場合は、ファームウェアのソース配布が不完全です。新しい依存ライブラリーをビルド中に取得する理由にはなりません。

## Windows

ソースのルートから、PowerShell 7またはWindows PowerShellをCMDラッパー経由で使用して実行してください。

```powershell
$env:PICO_SDK_PATH = '<external-sdk-directory>'
$env:SDK_TOOLCHAIN = '<arm-gnu-toolchain-directory>'
.\build-release.cmd -PythonExecutable '<python-interpreter>'
```

Pythonの選択順は `-PythonExecutable`、`Python3_EXECUTABLE`、`PYTHON_EXECUTABLE`、`PATH` 上の `python3` / `python` です。インタープリターの選択後、protobufの生成には既存の `build-release/venv` が優先されます。必要なPythonパッケージがない場合は、次のコマンドでローカル環境を明示的に準備してください。

```powershell
.\build-release.cmd -InstallPythonRequirements
```

pipが実行されるのはこのオプションを指定した場合だけです。通常のビルドはグローバルなPython環境にパッケージをインストールしません。新しい環境へのインストールにはインターネット接続が必要な場合があります。TLSエラーは有効なCA・プロキシ設定または取得済みのwheelで解決してください。`--trusted-host`、証明書検証の無効化、個人のマシンのパスで回避しないでください。オフラインでは次の方法で準備できます。

```powershell
python -m venv build-release/venv
build-release/venv/Scripts/python.exe -m pip install --no-index --find-links '<wheel-directory>' -r lib/nanopb/extra/requirements.txt
```

ラッパーは `-S <source-root> -B <source-root>/build-release`、`Release`、`SKIP_SUBMODULES=ON`、`SKIP_WEBBUILD=ON` を指定し、依存ソースの取得を無効にします。Ninjaがあれば使用し、なければ適切なMakeツールを使用します。`-Generator` でも指定できます。コンパイルの並列数は初期値1で、変更する場合は `-Jobs` を明示します。SDK・ツールチェーン・ジェネレーターを変更するときは、新しいビルドディレクトリーを使用してください。

## Webの再生成

完全なソース配布には生成済みのWebファイルシステムが含まれます。通常のファームウェアビルドにNode.js・npmは不要です。Webコンテンツの再生成が明示的に必要な場合だけ、次のコマンドを実行してください。

```powershell
.\build-release.cmd -RebuildWeb
```

このコマンドは `www` で `npm ci` と `npm run build` を実行し、生成済みのWebコンテンツを変更します。固定された依存パッケージの取得にはインターネット接続が必要な場合があります。この手順書を読むだけでReleaseビルドや実機への書き込みが行われることはありません。

## POSIX / GNU Make

GNU MakeはPOSIXシェルを使用します。WindowsのGit Bashも対象です。ネイティブの `cmd.exe` では代わりに `build-release.cmd` を使用してください。SDKの環境変数を設定し、指定したコンパイラーを `PATH` に配置します。

```sh
export PICO_SDK_PATH='<external-sdk-directory>'
export PICO_TOOLCHAIN_PATH='<toolchain-bin-directory>'
export picotool_DIR='<directory-containing-picotoolConfig.cmake>'
export PATH="$PICO_TOOLCHAIN_PATH:$PATH"
make release CMAKE=cmake PYTHON='<prepared-python-interpreter>'
make web
make test
```

`all` / `build` はWeb UIを再生成しません。`web` はnpmを明示的に使用します。`test` はネイティブのパーサー・共有計算テストを別の `build-native-tests` にビルドし、実機を使わないPythonのneGcon契約テストを実行します。ネイティブDLLテストには、各テストファイルで説明する明示的な有効化が必要です。`GENERATOR`、`JOBS`、`RELEASE_DATE` は上書きできます。

`make clean` にはPowerShellが必要です。初期値は `POWERSHELL=pwsh` です。指定されたビルド出力ディレクトリーだけを削除します。リンクされた出力ディレクトリーには使用しないでください。ラッパーはビルド時も削除時も、リンクされた `build-release` を拒否します。どちらのcleanターゲットもソース・同梱ライブラリー・外部の配布先を削除しません。

## 成果物と日付

ラッパーはReleaseのUF2を `build-release` だけに配置します。

```text
GP2040-CE_0.7.12-neGcon_SeeedXIAORP2040_yyyyMMdd.uf2
```

配布日の初期値はスクリプト実行時の実際のローカルOS日付です。`Get-Date -Format yyyyMMdd` を使用し、チャットの時刻やソースに固定した日付からは取得しません。このスクリプトの準備時に確認したOS日付は `20261029` でした。日付を明示する場合は `-ReleaseDate 20261029` または `make RELEASE_DATE=20261029` を指定します。この値は `GIT_REPO_BUILD_ID` にも使用されます。作成するリリースの実際の日付を選んでください。

日付入りのファイル名にコピーされるのはUF2だけです。中間ELF・map・protobuf・キャッシュ・venvはビルドディレクトリーに残り、ファームウェアの配布成果物には含めません。実機への書き込み、Gitのcommit・push、アーカイブの作成、外部の配布先へのコピーは行いません。初期値の `Release` ではデバッグビルドのDWARFを避けますが、ビット単位での再現性やバイナリー全体の個人情報監査を確認済みとはしていません。

## 任意のネイティブエディター

Windowsネイティブのプロファイルエディターは別のC++17ビルドであり、ファームウェアのビルドに必須ではありません。適切なコンパイラー環境から実行してください。

```sh
cmake -S pc-profile-editor -B build-native-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-native-tests --config Release --parallel 1
ctest --test-dir build-native-tests -C Release --output-on-failure
```

TXTプリセットのバージョン3と、それに対応するアナログプレビューABIバージョン3を組み合わせて使用してください。ネイティブテストやPCシミュレーションは、実機の動作、実際のUSBタイミング、最新ファームウェアの書き込み成功を確認するものではありません。
