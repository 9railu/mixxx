# RAIDJ タスク管理（簡易）

---

## 運用ルール

- **問題・調査が発生した場合** → `tasks/` フォルダにサブタスクファイルを作成
- **このファイルにはパスのみ記載**して内容の重複を避ける
- サブタスクファイル命名例：`tasks/TASK_rekordbox_cipher.md`
- 解決済みのサブタスクは完了欄にパスを移動して保持

---

## サブタスク一覧（未解決）

_なし_

---

## サブタスク一覧（解決済み）

- `tasks/TASK_sqlcipher_conflict.md` — SQLCipher × SQLite3 シンボル競合 → qsqlcipher-qt6 で解決

---

## 未決定事項（決定が必要）

- [x] Rekordbox DB インポートの呼び出し方法 → **C++ SQLite直読み + SQLCipher**（復号キーは pyrekordbox ソースから取得）
- [x] ドロップ/ブレイク検出アルゴリズム → **Mixxx 既存 FFT 流用 + RMS 閾値判定**（追加ライブラリなし・クールダウン付き）
- [x] エフェクト DSP ライブラリ → **Mixxx 既存 Signal Processing コード流用**（追加依存なし）
- [x] OBS WebSocket クライアント → **Qt6 QWebSocket 自前実装**（既存依存・追加不要）
- [x] Google Drive API クライアント → **QtNetwork で REST API 直叩き**（既存依存・追加不要）
- [x] VJ 映像なし時のフォールバック → **GLSL シェーダー**（GL Transitions インフラ流用・BPM/FFT を uniform で渡す）
- [x] VST3 対応方式 → **VST3 SDK 直接**（Steinberg 公式・GPL v3・個人利用無償）← フェーズ4

---

## 未決定事項（第2弾）

- [x] yt-dlp 配布形式 → **Python パッケージ**（pip・Python 環境は既に前提）
- [x] Demucs モデル DL元 → **Mixxx GSoC 2025 GitHub Releases**（ONNX変換済み・互換性確実。非公開時は Hugging Face からフォールバック）
- [x] フレキシブル BPM グリッド → **Essentia BeatTrackerMultiFeature**（最高精度・バックグラウンド処理なので重さは許容）
- [x] 波形カラー化 → **既存 WaveformRendererRGB を拡張**（`src/waveform/renderers/` に実装済み・ゼロから不要）
- [x] 1001Tracklists 書き出し → **なし**（対応しない）

---

## 未決定事項（第3弾）

- [ ] Qt6追加モジュール（websockets/network/multimedia）がMixxx公式スクリプトに含まれるか確認 → フォーク後に `tools/buildenv.py` を見て判断
- [ ] VJフォールバックGLSLシェーダーの取得元（glslsandbox / Shadertoy / 自作）← ライセンス確認も必要

---

## 進行中

_なし_

---

## 完了

- [x] ベースバージョン選定 → Mixxx 2.6
- [x] ビルド環境選定 → MSVC + vcpkg
- [x] ソフト名 → RAIDJ
- [x] GitHub Copilot CLI 動作確認 → `copilot` コマンド v1.0.54
- [x] ソースコード競合チェック（Essentia/ONNX/FFmpeg/NDI）
- [x] Rekordbox DB インポートの呼び出し方法 → C++ SQLite直読み + SQLCipher
- [x] ドロップ/ブレイク検出 → Mixxx既存FFT流用 + RMS閾値判定（クールダウン付き）
- [x] エフェクト DSP → Mixxx既存流用
- [x] OBS WebSocket → Qt6 QWebSocket自前実装
- [x] Google Drive API → QtNetwork REST直叩き
- [x] VJ フォールバック → GLSL シェーダー（GL Transitions基盤流用）
- [x] VST3 対応 → VST3 SDK直接（フェーズ4）
- [x] ライセンス方針 → 個人利用のため問題なし

---

更新：2026-06-04
