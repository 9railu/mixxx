# RAIDJ — Copilot CLI コンテキスト設定

## プロジェクト概要

RAIDJ は Mixxx 2.6（OSS DJ ソフト）をフォーク・拡張した個人用 DJ ソフトウェア。
Numark Party Mix II コントローラーで Rekordbox 並みの体験を実現する。
**個人利用・配布なし。**

## 技術スタック

- **言語**: C++17 / Qt6（Mixxx ベース）
- **ビルド**: Windows / MSVC 2022 / vcpkg / CMake
- **Mixxx ベース**: 2.6 beta ブランチ（`raidj-main` ブランチで開発）
- **Python**: yt-dlp・pyrekordbox（外部プロセス・埋め込みなし）

## コーディング規約（Mixxx に準拠）

- クラス名: `PascalCase`
- メンバ変数: `m_camelCase`
- 定数: `kCamelCase`
- シグナル: 動詞過去形 + 名詞（例: `trackLoaded`）
- スロット: `on + 動詞 + 名詞`（例: `onTrackLoaded`）
- スレッド境界をまたぐ受け渡しは `Qt::QueuedConnection`
- **オーディオスレッド内ではメモリアロケーション禁止**
- 新機能は `LibraryFeature` / `Analyzer` / `EffectProcessor` 等を継承する

## 主要ドキュメント（リポジトリ直下）

| ファイル | 内容 |
|---|---|
| `RAIDJ_PROJECT.md` | 目的・機能一覧・フェーズ計画 |
| `IMPROVEMENT_CHECKLIST.md` | 実装タスク詳細 |
| `MODULE_DESIGN.md` | クラス設計・追加場所・スレッド設計 |
| `DEPENDENCIES.md` | 依存ライブラリ一覧・競合情報 |
| `BUILD_SETUP.md` | ビルド環境構築手順 |
| `RISKS.md` | リスク・制約事項 |
| `TASKS.md` | 簡易タスク管理（未決定事項・サブタスク） |
| `tasks/` | 問題発生時のサブタスクファイル置き場 |

## 主要な追加モジュールと場所

```
src/library/youtube/          ← YouTube 統合（YouTubeFeature）
src/library/rekordboxdirect*  ← Rekordbox DB 直接インポート（SQLCipher使用）
src/analyzer/essentia*        ← Essentia BPM/Key 解析（BeatTrackerMultiFeature）
src/stem/                     ← ステム分離（Demucs v4 ONNX / QThreadPool）
src/effects/backends/builtin/raidj/  ← 追加エフェクト（Mixxx既存DSP流用）
src/vj/                       ← VJ エンジン（FFmpeg xfade + GL Transitions）
src/backup/                   ← Google Drive バックアップ
src/setlist/                  ← セトリ書き出し
```

## 既知の競合・注意事項

- **SQLCipher × SQLite3**: 同時リンク不可 → `qsqlcipher-qt6` で解決済み
- **Essentia × FFmpeg**: バージョン差異 → Essentia をスタティックビルドで分離済み
- **オーディオスレッド**: Demucs 推論・Essentia 解析は必ず `QThreadPool` で実行

## 実装フェーズ

- **フェーズ1**: ビルド環境・MIDI マッピング・Rekordbox インポート
- **フェーズ2**: 起動改善・Essentia 解析・ステム分離・YouTube 統合・波形・エフェクト
- **フェーズ3**: VJ・Google Drive・セトリ・スマートプレイリスト・UI 改善
- **フェーズ4**: VST3 対応・ストリーミング連携

## 問題が起きたとき

`tasks/TASK_TEMPLATE.md` をコピーして `tasks/TASK_xxx.md` を作成し、`TASKS.md` にパスを追記する。
