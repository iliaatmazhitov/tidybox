# TidyBox — Distributed File Watch & Delivery System

Текущий репозиторий содержит нативный C++ агент (наблюдение за файловой системой на macOS), серверную шину на Go и веб‑интерфейс на React — цель проекта превратить локальную утилиту наблюдения за файлами в распределённую систему доставки/распространения файлов и команд на подключённые устройства одного аккаунта.

Этот README полностью переписан и содержит новое техническое задание (ТЗ), архитектуру, формат сообщений, API, инструкции по сборке/развёртыванию, разделение задач по ролям, критерии приёмки и рекомендации по безопасности и эксплуатации.

Содержание
- Цели проекта
- Высокоуровневая архитектура
- Компоненты и их обязанности
- Формат данных и контракт API
- Потоки действий (flows)
- Детальное ТЗ: что реализовать в MVP
- Руководство по разработке и сборке
  - C++ агент (macOS)
  - Go сервер
  - React UI
  - CMake, сборка, запуск локально
- Развёртывание и инфраструктура
- Безопасность и приватность
- Мониторинг, логирование и тесты
- Разделение задач по ролям и acceptance criteria
- Roadmap и дальнейшие шаги
- Troubleshooting / FAQ

---

## Цели проекта

MVP:
1. Локальный C++ агент, подписывающийся на события файловой системы (macOS, FSEvents), формирует события и шлёт их на сервер.
2. Go сервер (hub) принимает события, буферизует и ретранслирует их клиентам через WebSocket; хранит и обслуживает файлы для загрузки (S3 или локально).
3. React UI: пользователь видит список подключённых устройств, может загрузить файл на все/выбранные устройства и отслеживать прогресс доставки.
4. Надёжная доставка: очереди задач, retry, оффлайн‑буферизация (сервер хранит задачу пока устройство не подключится).

Расширения (после MVP):
- Аутентификация/авторизация, multi‑account, групповая рассылка, дифференциальные обновления, P2P локальное распространение, Canary rollouts.

---

## Высокоуровневая архитектура

C++ agent (macOS, native)
  ↕ (HTTPS + WebSocket; TLS)
Go server (API, hub, job queue, storage)
  ↔ (WebSocket) React UI (browser)

Хранилище файлов: S3 (рекомендуется) или локальный storage.
Коммуникация:
- Agent → Server: HTTPS POST для событий; WebSocket (device connection) для job notifications and progress.
- Server → Agent: WS notifications (job_created).
- Server → UI: WS for device/job updates.
- File transfer: server presigned URL + agent downloads (agent‑pull).

---

## Компоненты и обязанности

1. C++ Agent
   - Наблюдение за файловой системой (FSEvents).
   - Группировка событий в батчи, сериализация в JSON.
   - Отправка батчей на сервер (HTTP POST) + опционально WebSocket для command/control.
   - Регистрация устройства и хранение device_token.
   - Прием job уведомлений от сервера: скачивание файла по presigned URL, проверка контрольной суммы, применение/сохранение, отчёт о прогрессе.

2. Go Server
   - REST API: регистрация устройств, приём событий, загрузка файлов, создание job'ов.
   - WS hubs: agents hub (device connections) и UI hub (user dashboard).
   - Job queue: создание/хранение/режим retry, отправка job notifications при подключении устройства.
   - Storage integration: S3 presigned URLs или локальное хранилище.
   - Auth (JWT), metrics (Prometheus), logging.

3. React UI
   - Учетная запись и аутентификация.
   - Devices list (online/offline).
   - Upload page: выбор файла, таргетинг устройств, создание job'а.
   - Jobs page: прогресс по каждому устройству, логи, ретраи.

---

## Формат данных и контракты (JSON)

Событие файловой системы (от агента к серверу) — пример:
```json
{
  "id": 1627845123456,
  "timestamp": "2025-09-30T10:00:00Z",
  "path": "/Users/ilya/Downloads/file.txt",
  "flags": 16,
  "flags_readable": ["ItemCreated"],
  "source": "agent-<uuid>",
  "meta": { "pid": 4321, "platform": "macos" }
}
```

Batch POST (агент → сервер):
POST /api/v1/events
Body:
```json
{ "device_id": "dev-123", "events": [ { ... }, { ... } ] }
```

Upload file (UI → сервер):
POST /api/v1/files (multipart/form-data)
Response:
```json
{ "file_id": 456, "filename": "app.zip", "size": 123456 }
```

Create jobs (UI → сервер):
POST /api/v1/jobs/batch
Body:
```json
{ "file_id":456, "device_ids":[1,2,3] } 
```
Response:
```json
{ "job_ids":[111,112,113] }
```

Server → Agent via WS (job notification):
```json
{
  "type":"job_created",
  "job": {
    "id": 111,
    "file": { "id":456, "url":"https://s3...signed", "filename":"app.zip", "size":123456, "sha256":"..." },
    "attempt":1
  }
}
```

Agent → Server via WS (progress/result):
```json
{ "type":"job_progress", "job_id":111, "downloaded":4096, "total":123456 }
{ "type":"job_result", "job_id":111, "status":"success", "log":"OK" }
```

---

## Потоки действий (flows)

1. Регистрация устройства
   - UI генерирует device token (или агент запрашивает у пользователся токен).
   - Agent сохраняет device_token и подключается к server WS `/ws/agent?token=...`.

2. Отправка событий
   - Agent собирает события, упаковывает в batch и POST /api/v1/events.
   - Server валидирует, сохраняет/индексирует и при необходимости отвечает.

3. Загрузка файла из UI на устройства
   - UI POST /api/v1/files → server сохраняет и генерирует presigned URL.
   - UI создаёт jobs (per device) POST /api/v1/jobs/batch.
   - Server enqueues job per device, отправляет WS notification to connected agents; offline devices получат job при следующем подключении.
   - Agent получает job, скачивает файл (presigned URL), отчитывается о прогрессе и результате.
   - Server агрегирует прогресс и рассылает обновления UI via WS.

---

## Детальное ТЗ (MVP)

C++ Agent
- Поддержка macOS (FSEvents) — стабильный eventCallback, безопасные преобразования eventPaths.
- Batch buffering: настройки max_batch_size, max_latency_ms.
- HTTP client (libcurl или cpp-httplib) для POST /api/v1/events и для скачивания файлов (resumable not required в MVP).
- WebSocket client (optional для control) — для получения job notifications, отправки progress/result.
- CLI: `agent --config agent.yml` с настройками (server_url, device_token, paths).
- Хранение device_token (в файле с 600 правами).
Acceptance:
  - агент стартует, не падает, отправляет события в JSON; при создании тестового файла на watched path сервер получает JSON.

Go Server
- REST endpoints:
  - POST /api/v1/devices/register
  - POST /api/v1/events
  - POST /api/v1/files
  - POST /api/v1/jobs/batch
  - GET /api/v1/devices, /api/v1/jobs
- WebSocket hubs:
  - /ws/agent — аутентификация device_token
  - /ws/ui — аутентификация UI JWT
- Job queue: DB-backed simple queue; worker to mark statuses and push notifications.
- Storage: S3 presigned URLs (or local storage as fallback).
Acceptance:
  - при загрузке файла и создании job сервер отправляет уведомления подключённым агентам, получает progress и job_result.

React UI
- Login + Devices list (online/offline) + Upload + Jobs pages.
- WebSocket connection to `/ws/ui` for real-time updates.
Acceptance:
  - Вы можете загрузить файл и наблюдать прогресс доставки по списку устройств.

Ожидаемые ограничения MVP:
- No code execution on agent without explicit user consent.
- No OTA installers in MVP — agent просто скачивает файл to local directory and marks job success.

---

## Сборка и разработка — локально

Требования:
- macOS (для сборки C++ агента и FSEvents); Xcode command line tools.
- Go 1.20+ (for server).
- Node 18+/npm 8+ (for React).
- CMake 3.15+.
- Optional: Docker for local stack (minio for S3, postgres/redis).

Репозиторная структура (рекомендуемая)
```
/agent          # C++ agent sources (CMake project)
 /src
 /CMakeLists.txt
/server         # Go server (modules)
 /cmd
 /internal
 /api
/ui             # React app (Create React App / Vite)
 /src
 /package.json
/docker-compose.yml
/CMakeLists.txt # top-level: builds agent for local dev
README.md
```

### C++ агент — сборка (macOS)
Пример CMakeLists.txt (агент):
- обеспечить линковку `-framework CoreServices` и `-lcurl` если используем libcurl.

Команды:
```bash
# в каталоге agent
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
# результат: ./agent (или имя бинарника)
```

Важно: при компиляции на macOS нужно линковать CoreServices:
```
target_link_libraries(agent "-framework CoreServices")
```

### Go сервер — запуск
```bash
cd server
go mod tidy
go run ./cmd/server
# или
go build -o bin/server ./cmd/server
./bin/server
```

Окружение (пример .env):
```
PORT=8080
DATABASE_URL=postgres://user:pass@localhost:5432/tidybox
S3_ENDPOINT=http://localhost:9000
S3_BUCKET=tidybox
JWT_SECRET=supersecret
```

### React UI — запуск
```bash
cd ui
npm install
npm start
# production build
npm run build
```

---

## CMake (top-level) — пример

Пример `CMakeLists.txt` (корень), чтобы собрать агент:
```cmake
cmake_minimum_required(VERSION 3.15)
project(tidybox)

# Build agent
add_subdirectory(agent)
```
(В `agent/CMakeLists.txt` — add_executable(agent src/main.cpp) и link CoreServices/curl)

---

## Развёртывание и инфраструктура

Рекомендуемая инфраструктура для production:
- Go server: Dockerized, deployable to Kubernetes or simple VM.
- Storage: S3 (AWS) or MinIO for on-prem.
- DB: Postgres for metadata; Redis for job queues (optional).
- TLS: Let's Encrypt / managed certs.
- CI: GitHub Actions pipelines for C++ (macOS runner for agent), Go (linux), React (linux).
- Monitoring: Prometheus + Grafana; logs to Loki/ELK.

---

## Безопасность и приватность

- TLS обязателен для серверных соединений и UI.
- Device tokens должны быть секретными; хранятся локально с правами 600.
- Пользователь должен давать явное согласие на отправку/получение файлов и доступ к защищённым каталогам (на macOS Full Disk Access).
- Files delivered should be verified (sha256) and optionally signed.
- Logging: avoid logging file contents or sensitive paths in production logs.

---

## Monitoring, Logging, Testing

- Expose `/metrics` для Prometheus (Go server).
- Structured JSON logs in server, rotate logs.
- Unit tests:
  - C++: serialization, batch-buffer behaviour.
  - Go: handlers (httptest), hub (websocket tests), job worker.
  - React: component tests (Jest + RTL).
- Integration tests:
  - Local docker-compose with server + minio + postgres + one mocked agent.
  - E2E scenario: upload file → create job → agent downloads → report.

---

## Разделение задач по ролям (кратко)

C++ dev (Agent)
- Implement FSEvents watch, safe eventCallback.
- Batching + HTTP POST + WS client + file downloader + CLI/config.
- Tests and CI.

Go dev (Server)
- REST API, WS hubs, job queue, DB model, S3 presigned URLs, metrics, auth.
- Tests, Dockerfile, CI.

React dev (Frontend)
- UI pages, WS client, upload and job creation UX, progress display.
- Tests and build pipeline.

DevOps
- CI pipelines (macOS runner for C++), docker-compose for local integration, deployment scripts.

QA
- Automated integration tests, load tests, security checks.

---

## Acceptance criteria (MVP checklist)

- [ ] Agent registers and stores device_token.
- [ ] Agent watches directories and sends valid JSON batches to server.
- [ ] Server accepts file uploads and creates presigned URLs.
- [ ] Server accepts job creation and queues job per device.
- [ ] Connected agents receive `job_created` notification and download file.
- [ ] Agent reports progress and result; server reflects status in UI.
- [ ] UI shows devices and job progress in real time.
- [ ] Basic auth in place (JWT), TLS in staging/prod.
- [ ] CI passes for all three components.

---

## Roadmap (next sprints)

Sprint 1 (MVP core)
- C++ agent: FSEvents + batching + HTTP POST.
- Go server: events endpoint, simple storage, job queue skeleton.
- React UI: devices list + simple WS client for job updates.
Estimated: 2–3 weeks across 3 developers.

Sprint 2 (Delivery)
- Agent WS, server WS hubs, presigned URL download, job progress reporting.
- UI: upload + jobs pages.
Estimated: 1–2 weeks.

Sprint 3 (Stability & Ops)
- Retry policies, queue persistence, metrics, CI.
Estimated: 1–2 weeks.

---

## Troubleshooting / FAQ

Q: Программа падает при запуске на macOS?
A:
- Убедитесь, что бинарник скомпилирован с `-framework CoreServices`.
- Проверьте права на конфиг и device_token (600).
- Для доступа к защищённым путям включите Full Disk Access в System Preferences.

Q: Агент не отправляет события
A:
- Включите подробный лог level.
- Убедитесь, что server_url корректен и сервер принимает POST /api/v1/events.
- Проверьте сетевые ограничения (firewall).

Q: Сервер не отправляет job уведомлений
A:
- Проверьте, что device подключён по WebSocket и аутентифицирован.
- Проверьте логи hub и DB (есть ли job в статусе queued).

---

## Примеры команд

Agent (dev):
```bash
# build
cd agent && mkdir -p build && cd build
cmake ..
make -j4

# run (example)
./agent --config ../configs/agent.yml
```

Server (dev):
```bash
cd server
go run ./cmd/server --config config.yml
```

UI (dev):
```bash
cd ui
npm install
npm start
```

---

## Что я сделал и что дальше

Я полностью переписал README: описал новое техническое задание, архитектуру, форматы сообщений, API контракты, инструкции по сборке и развёртыванию, разделил обязанности между разработчиками и предложил roadmap и acceptance criteria — всё в одном документе, чтобы команда могла сразу начать работу по MVP.

Дальше я могу:
- Сгенерировать OpenAPI (Swagger) спецификацию для REST API (devices, events, files, jobs).
- Создать шаблонный skeleton для Go server (handlers + WS hub) и минимальный C++ agent skeleton (FSEvents + POST).
- Создать детальные GitHub issues с чек-листами и estimate для каждой роли.

Я уже готов начать генерировать OpenAPI и/или шаблоны кода — скажите, с чего предпочитаете начать (OpenAPI / Go skeleton / C++ skeleton / React skeleton), и я сразу создаю нужные файлы.
