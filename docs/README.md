# neGcon 日本語ドキュメント

このフォルダーはGP2040-CE `0.7.12-neGcon` 配布の日本語案内です。元の文書とライセンスは保持し、日本語版は別名で用意しています。

## 日本語版

- [ソースのビルド](BUILD-neGcon-ja.md): 外部ツール、Windows・GNU Makeの手順、Web再生成、成果物の日付。
- [変更内容](CHANGES-neGcon-ja.md): neGconの初期値、感圧加算、TXT v3、保存設定の互換性、検証の範囲。
- [Switch 2接続修正](SWITCH2-USB-FIX-ja.md): USBアドレス更新、Switch／Switch Proの実機確認と残る検証範囲。
- [PC版TXTプロファイルエディター](PC-PROFILE-EDITOR-ja.md): オフライン編集、配布用 `bin` とソースのビルド出力、TXT v1/v2/v3、保存と検証。

## 原文と共通仕様

- [ビルド手順の英語原文](BUILD-neGcon.md)
- [変更内容の英語原文](CHANGES-neGcon.md)
- [TXTプリセットの共通仕様](../preset-format/SPECIFICATION.md)
- [PCエディター実装元README](../pc-profile-editor/README.md): 旧v2の記載があります。現在の形式は共通仕様と `../pc-profile-editor/src/preset.cpp` のv3実装を参照してください。
- [neGconのルート案内](../README-neGcon.md)
- [元のルートREADME](../README.md)
- [ルートLICENSE](../LICENSE)

PCエディターの配布先は `../pc-profile-editor/bin/psx_profile_editor.exe` です。
本ソースおよびDocumentにはgpt6.1-solによって生成されたコードを含みます。
