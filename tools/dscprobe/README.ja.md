[English](README.md) | 日本語

# DscProbe

Windows 11 ARM64 向けの、一度だけ実行する**読み取り専用**カーネル
プローブ。表示が生きている状態で、spacewar 上の `qcdxkm7325.sys` の
表示／DSC 状態を調べるために使う。ハードウェア、対象ドライバのメモリ、
レジストリ、ネットワークへの書き込みは一切行わず、書くファイルは自分の
レポート `C:\Windows\DscProbe_out.txt`（プレーンな `key=value` 形式）
だけ。

現在のバージョンは **v3.8**。以前の版が読んでいたレジスタ集合（DSC
エンコーダブロック、DSC／CTL／INTF の状態、ワードカウント、DSC
レコード）に加えて、次を読む：

- panel-info の DSC フィールド群（DSC 有効化の主フラグ、モード
  インデックス、固定ビットクロック上書き値、スライス内バイト数、
  スライス数、DSC 設定ブロックの生ダンプ）
- DSI のエラー／ステータスレジスタ集合（FIFO ステータス、ACK エラー、
  PHY エラー、タイムアウト、エラー割り込みマスク、割り込み制御、
  クロックステータス）。Windows セッションを、動作しているメインライン
  Linux セッションと比較できるようにするため。

`qcdxkm7325.sys` 内の固定 RVA を使うので、対象は
[`../../patches`](../../patches) で同定される正確なドライバビルドに
限られる。すべての参照は境界チェックとガード付きで、結果は段階的に
書き出すため、後段のガードで失敗しても先に読んだ分は失われない。

## ビルド

ARM64 ターゲットの EWDK `LaunchBuildEnv` シェルから：

```bat
build.bat
sign.bat   :: sign.bat 内の証明書は自分のテスト証明書に置き換えること
```

## 使い方

`DscProbe.sys` を電話へコピーし（例：`C:\DscProbe\DscProbe.sys`）、
グラフィックスデバイスを有効化して表示が生きている状態で、管理者
プロンプトから：

```bat
sc.exe create DscProbe type= kernel start= demand binPath= C:\DscProbe\DscProbe.sys
sc.exe start DscProbe
type C:\Windows\DscProbe_out.txt
sc.exe stop DscProbe
sc.exe delete DscProbe
```
