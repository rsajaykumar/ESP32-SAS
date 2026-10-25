# 🎓 Smart Attendance System with ESP32 & Face Verification

> 💡 A web-based smart attendance management system that combines **ESP32-based device identification, facial recognition, Firebase Firestore, and role-based portals** for HODs, teachers, and students.

<p align="center">
  <strong>📡 Smart Identification • 🧑‍💻 Face Verification • ☁️ Cloud Database • 📊 Attendance Management</strong>
</p>

---

## 📑 Table of Contents

| # | Section |
|---|---|
| 01 | [📌 Overview](#-1-overview) |
| 02 | [🎯 Objectives](#-2-objectives) |
| 03 | [✨ Features](#-3-features) |
| 04 | [🏗️ System Architecture](#️-4-system-architecture) |
| 05 | [🛠️ Technologies Used](#️-5-technologies-used) |
| 06 | [📂 Repository Structure](#-6-repository-structure) |
| 07 | [💻 Requirements](#-7-requirements) |
| 08 | [⚙️ Installation and Setup](#️-8-installation-and-setup) |
| 09 | [☁️ Firebase Configuration](#️-9-firebase-configuration) |
| 10 | [🧑‍🎓 Student Face Enrollment](#-10-student-face-enrollment) |
| 11 | [🚀 Running the Application](#-11-running-the-application) |
| 12 | [🔄 How the System Works](#-12-how-the-system-works) |
| 13 | [🔌 API Reference](#-13-api-reference) |
| 14 | [🧪 Testing](#-14-testing) |
| 15 | [🛠️ Troubleshooting](#️-15-troubleshooting) |
| 16 | [🔐 Security Considerations](#-16-security-considerations) |
| 17 | [⚠️ Limitations](#️-17-limitations) |
| 18 | [🚀 Future Improvements](#-18-future-improvements) |
| 19 | [©️ License](#️-19-license) |

---

## 📌 1. Overview

Traditional attendance systems often depend on manual roll calls or paper registers. These methods can consume valuable classroom time and make attendance management more difficult.

**The Smart Attendance System** aims to simplify this process through a browser-based application that combines student management, attendance tracking, device identification, and facial verification.

### 🌟 What does this project provide?

- 👨‍💼 **HOD Portal** — Manage students, teachers, subjects, schedules, and attendance records.
- 👩‍🏫 **Teacher Portal** — Select subjects and periods, and control attendance sessions.
- 🧑‍🎓 **Student Portal** — View overall and subject-wise attendance information.
- 📸 **Face Verification** — Capture a camera image and submit it for identity verification.
- 📡 **Device Identification** — Use registered MAC addresses in the attendance workflow.
- ☁️ **Cloud Data Storage** — Use Firebase Firestore for student enrollment data.

The frontend communicates with a Python backend through HTTP API endpoints. The provided `enroll.py` script uses the Firebase Admin SDK to store student details and facial encodings in Firestore.

> ℹ️ **Implementation note:** The frontend and enrollment script have been reviewed. The exact backend workflow, ESP32 communication, runtime face-matching logic, and attendance data storage must be confirmed from the corresponding source files.

## 🎯 2. Objectives

The main objectives of this project are to:

- ✅ Reduce the effort involved in manual attendance management.
- 👥 Provide separate portals for HODs, teachers, and students.
- 🪪 Register students using unique IDs and device MAC addresses.
- 📸 Store facial encodings for identity verification.
- 📅 Support attendance sessions organized by subject and period.
- 📊 Display overall and subject-wise attendance summaries.
- ☁️ Store student enrollment information in Firebase Firestore.
- 🧰 Provide documentation that helps another engineer install, configure, and test the system.

## ✨ 3. Features

### 👨‍💼 HOD Portal

The HOD interface includes the following management functions:

- ➕ Register students with a student ID, name, and MAC address.
- 👥 View registered students and connected-device information.
- 🗑️ Delete student records.
- 👩‍🏫 Add and remove teachers.
- 📚 Create and delete subjects.
- 🧑‍🏫 Associate teachers with subjects.
- 🗓️ Configure subject schedules and period timings.
- 📋 View attendance records and summaries.
- 📸 View recorded attendance photographs.
- 🧹 Delete individual records or clear attendance records.

### 👩‍🏫 Teacher Portal

Teachers can use the interface to:

- 📚 View available subjects.
- 🗓️ Review subject schedules.
- 🕒 Select a period.
- ▶️ Start an attendance session.
- ⏹️ Stop an attendance session.
- 🔄 View the current session status.

### 🧑‍🎓 Student Portal

The student interface provides:

- 🔎 Student lookup using a student ID.
- 📊 Overall attendance statistics.
- 📚 Subject-wise attendance summaries.
- 📷 Browser camera access.
- 🧠 Submission of captured images for facial verification.
- 📝 Attendance marking through the backend API.
- ⚠️ Feedback when recognition or attendance marking fails.

### 🧠 Face Enrollment

The Python enrollment utility performs the following tasks:

1. 🪪 Accepts the student's ID, name, MAC address, and photograph path.
2. 🖼️ Checks whether the photograph exists.
3. 🔍 Detects and encodes a face using `face_recognition`.
4. 🔢 Converts the face encoding into a list.
5. 📡 Normalizes the MAC address to uppercase.
6. ☁️ Stores the student details and face encoding in Firestore.

---

## 🏗️ 4. System Architecture

The following diagram illustrates the intended high-level architecture. Some connections still require verification against the backend and ESP32 firmware.

```text
                 👨‍💼 HOD / 👩‍🏫 TEACHER / 🧑‍🎓 STUDENT
                                  |
                                  v
                         🌐 Browser Interface
                           HTML / CSS / JS
                                  |
                                  v
                            🔌 HTTP APIs
                                  |
                                  v
                         🐍 Python Backend
                              server.py
                                  |
                  +---------------+---------------+
                  |               |               |
                  v               v               v
             🔐 Login       🧠 Face Check    📝 Attendance
                  |               |               |
                  +---------------+---------------+
                                  |
                                  v
                           ☁️ Firestore
                                  ^
                                  |
                            🧑‍🎓 enroll.py
                                  ^
                                  |
                         📸 Student Photograph

                 📡 ESP32 Device Identification
                    [Integration to verify]
```

### 🧩 Main Components

| Component | Responsibility |
|---|---|
| `index.html` | 🌐 Web interface and portal elements |
| `style.css` | 🎨 Styling, responsive layout, tables, and dialogs |
| `app.js` | ⚡ Frontend logic, API calls, camera capture, and attendance requests |
| `server.py` | 🐍 Backend entry point; implementation must be verified |
| `enroll.py` | 🧠 Extracts facial encodings and stores enrollment data |
| Firebase Firestore | ☁️ Stores enrollment data written by `enroll.py` |
| ESP32 firmware | 📡 Intended device identification component; exact behavior depends on the firmware |

## 🛠️ 5. Technologies Used

| Technology | Purpose |
|---|---|
| 🌐 HTML5 | Web page structure |
| 🎨 CSS3 | Styling and responsive layout |
| ⚡ JavaScript | Frontend interactions and HTTP requests |
| 🐍 Python | Enrollment and backend processing |
| 🔥 Firebase Admin SDK | Server-side Firebase access |
| ☁️ Cloud Firestore | Student enrollment storage |
| 🧠 `face_recognition` | Facial encoding extraction |
| 📷 Browser MediaDevices API | Camera access |
| 📡 ESP32 | Device-side attendance functionality, subject to firmware verification |

## 📂 6. Repository Structure

The following structure is recommended for organizing the project on GitHub.

```text
smart-attendance-system/
│
├── 📄 README.md
├── 📄 requirements.txt
├── 📄 .gitignore
│
├── 📁 frontend/
│   ├── index.html
│   ├── style.css
│   └── app.js
│
├── 📁 backend/
│   └── server.py
│
├── 📁 enrollment/
│   └── enroll.py
│
├── 📁 esp32/
│   └── SmartAttendance.ino
│
├── 📁 docs/
│   ├── architecture.md
│   ├── installation.md
│   ├── api-reference.md
│   ├── enrollment-guide.md
│   ├── testing.md
│   └── troubleshooting.md
│
└── 📁 assets/
    ├── screenshots/
    └── diagrams/
```

> 📝 **Note:** This is a suggested layout. Keep the original paths if your backend expects the files in different locations. Update file paths and static-file configuration together if you reorganize the project.

## 💻 7. Requirements

### 🔌 Hardware

- 💻 A computer running a supported operating system.
- 📷 A webcam or compatible camera.
- 📡 An ESP32 development board for the device-based attendance component.
- 📶 A Wi-Fi network for the relevant devices and server.
- 📱 Student devices, if the ESP32 firmware uses connected-device MAC addresses.

### 🧰 Software

- 🐍 Python compatible with the selected dependencies.
- 📦 pip package manager.
- 🔧 Git.
- 🌐 A modern web browser.
- 🔥 A Firebase project with Cloud Firestore enabled.
- 🔑 Firebase service-account credentials.
- 🛠️ Arduino IDE or a compatible ESP32 development environment, if required.

The supported Python version and ESP32 board package version should be confirmed through testing.

## ⚙️ 8. Installation and Setup

Follow these steps to prepare a local development environment.

### Step 1️⃣ — Install Python and Git

Install Python and Git, then open a terminal or command prompt.

Verify the installations:

```bash
python --version
python -m pip --version
git --version
```

On systems where Python is invoked as `python3`, use that command instead.

### Step 2️⃣ — Clone the Repository

After uploading the project to GitHub, run:

```bash
git clone <YOUR_REPOSITORY_URL>
cd smart-attendance-system
```

Replace `<YOUR_REPOSITORY_URL>` with your actual repository URL.

### Step 3️⃣ — Create a Virtual Environment

**Windows PowerShell:**

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
```

**Windows Command Prompt:**

```bat
python -m venv .venv
.venv\Scripts\activate.bat
```

**Linux:**

```bash
python3 -m venv .venv
source .venv/bin/activate
```

A virtual environment helps keep this project's Python dependencies separate from other projects.

### Step 4️⃣ — Install Dependencies

Install the packages listed in the project's verified `requirements.txt`:

```bash
python -m pip install --upgrade pip
pip install -r requirements.txt
```

The supplied enrollment script requires at least:

```text
firebase-admin
face-recognition
```

⚠️ This is only a starting list, not a verified, complete dependency file. The backend may need additional packages. The `face_recognition` library also depends on `dlib`, which may require additional installation steps depending on your operating system and Python version.

### Step 5️⃣ — Configure Firebase

Follow Section 9 before running the enrollment utility.

### Step 6️⃣ — Verify the Project Files

Ensure the frontend, backend, and enrollment files are in their configured locations. Check the backend's static-file configuration and its Firebase credential path.

### Step 7️⃣ — Start the Application

Use the startup command appropriate for the actual implementation of `server.py`. See Section 11.

---

## ☁️ 9. Firebase Configuration

The supplied `enroll.py` initializes Firebase using the following code:

```python
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred)
db = firestore.client()
```

This expects a Firebase service-account credential file at the configured relative path.

### 🔧 Setup Procedure

1. 🔥 Create or select your Firebase project.
2. ☁️ Enable Cloud Firestore.
3. 🔑 Generate a service-account credential through the appropriate Firebase or Google Cloud project settings.
4. 💻 Store the credential securely in your local development environment.
5. 📂 Ensure the path used by `enroll.py` points to the credential file.
6. 🔐 Verify that the service account has the necessary Firestore permissions.

The enrollment script writes student documents to the `students` collection, using the supplied student ID as the document ID.

### 🗃️ Example Student Document

The following example illustrates the data structure. The short encoding shown is illustrative and is not a complete face encoding.

```json
{
  "id": "STU001",
  "name": "Example Student",
  "mac": "AA:BB:CC:DD:EE:FF",
  "faceEncoding": [
    0.12,
    -0.08,
    0.03
  ],
  "registeredAt": "YYYY-MM-DD HH:MM:SS"
}
```

### 🔐 Important Security Rules

- Never upload `serviceAccountKey.json` to GitHub.
- Never publish Firebase private keys, passwords, or access tokens.
- Keep credentials and sensitive configuration in your private local environment.
- Do not expose service-account credentials in frontend JavaScript.
- Exclude private student photographs, face encodings, and attendance data from the public repository.

**This project does not require sharing `.env` or `.env.example` files.** Critical configuration should remain private.

---

## 🧑‍🎓 10. Student Face Enrollment

The `enroll.py` utility registers a student and stores their facial encoding in Firestore.

### 🖼️ Step 1 — Prepare a Photograph

Choose a clear photograph containing one visible face. Make sure the file exists and that Python can read it.

### ⌨️ Step 2 — Run the Enrollment Script

The command format is:

```bash
python enroll.py <studentId> <name> <mac> <imagePath>
```

Example:

```bash
python enroll.py STU001 "Example Student" AA:BB:CC:DD:EE:FF student.jpg
```

Replace the example values with the intended student's details and image path.

### 🧠 Step 3 — Understand the Process

The script:

1. Checks whether the image exists.
2. Loads the image.
3. Extracts face encodings.
4. Stops if no face is detected.
5. Selects the first returned face encoding.
6. Converts the encoding into a list.
7. Normalizes the MAC address to uppercase.
8. Writes the student record to Firestore.

### ✅ Step 4 — Verify Enrollment

A successful run prints a message similar to:

```text
Enrolled: STU001 | Example Student | AA:BB:CC:DD:EE:FF
```

Open the `students` collection in Firestore to check whether the document was created.

### ⚠️ Important Behavior

- If no face is detected, the script does not write the enrollment document.
- If multiple faces are present, the script uses the first encoding returned.
- Re-enrolling the same student ID can update the existing document because `merge=True` is used.
- Duplicate MAC addresses are not explicitly checked by the supplied script.
- The credential path is relative to the process's working directory.
- The registration timestamp is stored as a formatted string generated from `datetime.now()`.

---

## 🚀 11. Running the Application

The precise startup command depends on the implementation of `server.py`, so it must be verified before documenting a final command.

### ▶️ Recommended Startup Sequence

1. Activate the Python virtual environment.
2. Confirm the local Firebase credential configuration.
3. Install the verified dependencies.
4. Start the Python backend using its actual entry point.
5. Open the application through its configured web address.
6. Test HOD login and student management.
7. Test teacher subject and attendance-session controls.
8. Test student lookup and face verification.
9. Test the ESP32 component and its integration separately.

### 📷 Browser Camera Requirements

The frontend requests camera access using:

```javascript
navigator.mediaDevices.getUserMedia({
  video: { facingMode: "user" }
});
```

Camera access generally requires a secure context, such as HTTPS, or localhost during development. The user must also grant browser camera permission.

> 💡 Opening `index.html` directly through `file://` may not work because the frontend calls relative API routes such as `/api/login`, `/api/data`, and `/api/recognize`. Use the application's configured HTTP server.

## 🔄 12. How the System Works

### 🔐 A. Login and Authentication

The frontend provides separate login flows for HODs and teachers. After login, the JavaScript stores the returned token in memory and sends it with subsequent data and command requests.

The backend must enforce authorization for every protected operation.

### 🔎 B. Student Lookup

The student enters a student ID. The frontend requests `/api/student` and displays the returned student details and attendance summaries.

### 📸 C. Face Verification

The supplied frontend follows this sequence:

1. Requests browser camera access.
2. Captures an image on a canvas.
3. Converts the captured frame into a JPEG data URL.
4. Sends the image to `/api/recognize`.
5. Processes the recognition response.
6. If recognition succeeds, submits an attendance request to `/api/mark`.

The frontend includes `studentId`, `mac`, `faceMatched`, `subject`, `period`, and `photoFilename` in the attendance request.

🔒 **Security requirement:** The backend must independently verify the recognition result and the student's identity. It must not rely only on a client-supplied `faceMatched` value.

### 📝 D. Attendance Management

The teacher selects a subject and period and can start or stop an attendance session through `/api/cmd`.

The HOD interface includes management commands for students, teachers, subjects, schedules, and attendance records.

### ☁️ E. Enrollment and Firestore

The separate enrollment utility stores student information and face encodings in Firestore. The exact runtime process for retrieving those encodings and comparing them against a captured image must be verified from `server.py`.

---

## 🔌 13. API Reference

The following endpoints are visible in the supplied JavaScript. Their exact methods, validation rules, and server-side behavior must be checked against `server.py`.

| Endpoint | Purpose |
|---|---|
| `GET /api/login` | HOD or teacher login request in the current frontend |
| `/api/data` | Retrieve portal data |
| `/api/cmd` | Execute management or attendance-session commands |
| `/api/student` | Look up student and attendance information |
| `POST /api/recognize` | Submit a captured image for recognition |
| `POST /api/mark` | Request attendance marking |
| `/api/photo` | Retrieve an attendance photograph |

### 🔑 Login Request

The current HOD login flow sends a request resembling:

```text
/api/login?role=hod&pass=<password>
```

The teacher login flow includes the role, teacher ID, and password.

⚠️ For production use, credentials should not be placed in URL query parameters. Use HTTPS and a suitable POST-based authentication flow to reduce the risk of credentials appearing in browser history or server logs.

### ▶️ Attendance Command

The frontend builds command requests resembling:

```text
/api/cmd?token=<token>&cmd=start&subject=<code>&period=<period>
```

The actual supported commands and authorization rules must be verified against the backend.

### 📸 Recognition and Attendance Marking

The frontend sends a captured JPEG data URL to `/api/recognize` using POST.

If recognition succeeds, it submits JSON to `/api/mark`. The current frontend uses `General` as the subject and `Face-Verify` as the period in that particular flow. Confirm these values against the intended attendance workflow.

---

## 🧪 14. Testing

Use the following checklists when setting up the project on a new computer.

### 🧠 Enrollment Tests

- [ ] A valid photograph loads successfully.
- [ ] A missing image path produces an error.
- [ ] An image with no detectable face is rejected.
- [ ] A valid student document is created in Firestore.
- [ ] The MAC address is normalized to uppercase.
- [ ] Re-enrollment behavior is verified.

### 🌐 Frontend Tests

- [ ] The application loads through the configured HTTP server.
- [ ] Invalid login credentials are rejected.
- [ ] HOD and teacher login work correctly.
- [ ] Unknown student IDs are handled correctly.
- [ ] Subjects and schedules display correctly.
- [ ] Attendance records and summaries display correctly.

### 📸 Face Verification Tests

- [ ] Camera permission can be granted.
- [ ] An image can be captured and submitted.
- [ ] An unknown face is rejected.
- [ ] A known face is matched correctly.
- [ ] Attendance marking requires server-side verification.
- [ ] MAC mismatches are handled correctly.
- [ ] Camera and network failures produce useful error messages.

### 📡 Device Integration Tests

- [ ] ESP32 connectivity is confirmed.
- [ ] Device MAC addresses are read correctly.
- [ ] Registered and unregistered devices are distinguished.
- [ ] Device behavior matches the intended attendance workflow.
- [ ] Disconnection and reconnection are tested.

> ✅ Mark each item complete only after testing it on the actual implementation.

## 🛠️ 15. Troubleshooting

| Problem | What to check |
|---|---|
| 🔑 Firebase credential error | Check the local credential file path and permissions. |
| ☁️ Firestore access failure | Verify the Firebase project, Firestore setup, and service-account permissions. |
| 📸 No face detected | Try a clear, well-lit photograph containing one visible face. |
| 📦 `face_recognition` installation failure | Check Python compatibility and `dlib` build dependencies. |
| 🎥 Browser camera failure | Check browser permissions and use localhost or HTTPS. |
| 🔐 Login failure | Check backend logs, credentials, and route implementation. |
| 🌐 API connection failure | Confirm that the backend is running and the frontend uses the correct server origin. |
| 📝 Attendance not marked | Inspect `/api/recognize` and `/api/mark` responses and backend validation. |
| 📡 MAC mismatch | Compare the registered MAC address with the value received by the backend. |
| 📚 Subject data missing | Check the actual backend storage and data-loading logic. |
| 🖼️ Photograph unavailable | Check the filename, storage location, and `/api/photo` authorization rules. |

These are initial troubleshooting checks. Add exact error messages and confirmed fixes after testing the complete application.

---

## 🔐 16. Security Considerations

This project handles student identities, biometric data, device identifiers, and attendance records. Protecting this information is essential.

### 🛡️ Essential Security Practices

- 🔑 Never commit Firebase service-account credentials.
- 🚫 Do not publish passwords, private keys, or access tokens.
- 🗂️ Exclude private photographs, face encodings, databases, and attendance data from Git.
- 🔒 Use HTTPS outside localhost.
- 🔐 Store passwords securely using an appropriate password-hashing method.
- 👥 Enforce role-based authorization on the backend.
- 🧠 Validate face-recognition results on the server.
- 🧹 Validate and sanitize incoming request parameters.
- 🔏 Restrict access to student records and attendance photographs.
- 🗑️ Define appropriate retention and deletion procedures for biometric information.
- 🤝 Obtain appropriate consent and restrict access to authorized personnel.
- 📝 Avoid exposing credentials in URLs, logs, or error messages.

### ⚠️ Face Verification Is Not the Same as Liveness Detection

A matching face image does not prove that a live person is physically present. A photograph or replayed image may be used to attempt to bypass basic recognition.

Stronger protection may require liveness detection, replay protection, and additional server-side safeguards.

## ⚠️ 17. Limitations

The following limitations are apparent from the supplied code or remain to be verified:

- 🧠 The enrollment script uses only the first face encoding returned for an image.
- 📡 Duplicate MAC addresses are not explicitly rejected by the enrollment script.
- 📸 The frontend captures a single image rather than performing a verified liveness test.
- 🔑 The frontend stores the login token in JavaScript memory.
- 🔍 The runtime face-matching algorithm and its thresholds require backend verification.
- 🔌 The exact ESP32-to-backend communication method requires firmware review.
- ☁️ Attendance-record persistence and synchronization require backend verification.
- 📚 The face-verification marking request uses fixed subject and period values in the current frontend.
- 🛡️ Production deployment requires further security and privacy validation.

## 🚀 18. Future Improvements

Possible improvements for future versions include:

- 🔐 Stronger authentication and role-based access controls.
- 🧠 Liveness detection and anti-spoofing safeguards.
- 📚 Configurable subject and period selection during face-based attendance.
- 📡 Duplicate student and MAC-address validation.
- 📋 Better application logs and error reporting.
- 🧪 Automated tests for authentication, enrollment, and attendance marking.
- 📊 Attendance exports and advanced reporting.
- 🗑️ Improved biometric-data retention and deletion controls.
- 🌐 A deployment guide for local networks and secure remote hosting.

These are potential improvements, not claims that they are already implemented.

---

## ©️ 19. License

This project is currently **unlicensed**. All rights to the source code are reserved by the project owner. No permission is granted to copy, modify, distribute, or use this project without the owner's explicit permission.

If you wish to reuse or redistribute the code, contact the project owner for authorization.

---

<p align="center">
  <strong>🎓 Smart Attendance System</strong><br>
  <sub>📡 Device Identification • 🧠 Face Verification • ☁️ Cloud-Based Enrollment • 📊 Attendance Management</sub>
</p>

<p align="center">
  <em>Built to explore smarter, more organized attendance management.</em>
</p>
