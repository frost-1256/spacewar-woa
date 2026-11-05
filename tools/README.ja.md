[English](README.md) | 日本語

# ツール

## dscprobe/

**DscProbe** のソース。稼働中セッションでグラフィックスドライバの
表示／DSC 状態を調べるための読み取り専用カーネルプローブ。
[dscprobe/README.ja.md](dscprobe/README.ja.md) を参照。

## phone/

電話側（`C:\`）に置いて管理者として実行する補助スクリプト：

| ファイル | 用途 |
|---|---|
| `gpu_now.bat` / `gpu_now.ps1` | GPU デバイスを手動で有効化し、状態を `C:\gpu_now.log` へ 5 分間ポーリング記録する。**後片付けはしない** — テスト後は自分で無効化し、再起動前に `ConfigFlags` が 1 に戻っていることを確認すること（[../STATUS.ja.md](../STATUS.ja.md) 参照）。 |
| `kd_on.bat` | カーネルデバッグを有効化（`bcdedit /debug on`）。WinDbg／KDNET セッション用。 |
| `kd_off.bat` | カーネルデバッグを無効化（既定）。オンのままホストが居ないと、ブートが KD トランスポート待ちで止まる。 |
