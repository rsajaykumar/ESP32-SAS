import os
import io
import base64
import numpy as np
import time
import requests
from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import firebase_admin
from firebase_admin import credentials, firestore, db
import face_recognition
from datetime import datetime

# ============================================================
# FIREBASE INITIALIZATION
# ============================================================
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://authentication-303f8-default-rtdb.firebaseio.com/'
})

firestore_db = firestore.client()
rtdb = db.reference()

app = Flask(__name__)
CORS(app, resources={r"/api/*": {"origins": "*"}})

FACE_CACHE = {}

def load_faces_from_firebase():
    global FACE_CACHE
    FACE_CACHE = {}
    print("[AI] Loading face encodings from Firestore...")
    try:
        students_ref = firestore_db.collection('students').stream()
        count = 0
        for doc in students_ref:
            data = doc.to_dict()
            encoding = data.get('faceEncoding')
            if encoding and len(encoding) == 128:
                FACE_CACHE[doc.id] = {
                    "name": data.get("name", "Unknown"),
                    "mac": data.get("mac", "").strip().upper(),
                    "encoding": np.array(encoding)
                }
                count += 1
        print(f"[AI] Successfully loaded {count} student faces into memory.")
    except Exception as e:
        print(f"[AI] Error loading faces: {e}")

@app.route('/api/recognize', methods=['POST'])
def recognize():
    img_bytes = None
    if request.data:
        try:
            b64_string = request.data.decode('utf-8')
            if ',' in b64_string:
                b64_string = b64_string.split(',')[1]
            img_bytes = base64.b64decode(b64_string)
        except Exception as e:
            return jsonify({"ok": False, "msg": "Invalid base64 image"}), 400

    if not img_bytes:
        return jsonify({"ok": False, "msg": "No image provided"}), 400

    try:
        image = face_recognition.load_image_file(io.BytesIO(img_bytes))
        face_locations = face_recognition.face_locations(image)
        face_encodings = face_recognition.face_encodings(image, face_locations)

        if len(face_encodings) == 0:
            return jsonify({"ok": False, "msg": "No face detected. Please ensure good lighting."})
        if len(face_encodings) > 1:
            return jsonify({"ok": False, "msg": "Multiple faces detected. Please ensure you are alone."})

        unknown_encoding = face_encodings[0]
        if len(FACE_CACHE) == 0:
            return jsonify({"ok": False, "msg": "No students registered yet."})

        known_encodings = [data["encoding"] for data in FACE_CACHE.values()]
        student_ids = list(FACE_CACHE.keys())

        face_distances = face_recognition.face_distance(known_encodings, unknown_encoding)
        best_match_index = np.argmin(face_distances)
        best_distance = face_distances[best_match_index]

        if best_distance < 0.5:
            matched_id = student_ids[best_match_index]
            matched_data = FACE_CACHE[matched_id]
            
            if not os.path.exists("captures"):
                os.makedirs("captures")
            
            filename = f"{matched_id}_{int(time.time())}.jpg"
            filepath = os.path.join("captures", filename)
            with open(filepath, "wb") as f:
                f.write(img_bytes)
            
            print(f"[STORAGE] Saved selfie: {filename}")

            return jsonify({
                "ok": True,
                "studentId": matched_id,
                "name": matched_data["name"],
                "mac": matched_data["mac"],
                "photoFilename": filename
            })
        else:
            return jsonify({"ok": False, "msg": "Face not recognized.", "distance": float(best_distance)})

    except Exception as e:
        print(f"[AI] Error processing image: {e}")
        return jsonify({"ok": False, "msg": "Internal server error"}), 500

@app.route('/captures/<filename>')
def get_capture(filename):
    return send_from_directory('captures', filename)

@app.route('/api/mark', methods=['POST'])
def mark_attendance():
    data = request.get_json()
    student_id = data.get('studentId')
    mac = data.get('mac', '').strip().upper()
    face_matched = data.get('faceMatched', False)
    photo_filename = data.get('photoFilename', '')

    if not student_id:
        return jsonify({"ok": False, "msg": "Missing student ID"}), 400

    try:
        mac_connected = False
        if mac:
            safe_mac = mac.replace(":", "_")
            device = rtdb.child('liveDevices').child(safe_mac).get()
            if device:
                mac_connected = True
            else:
                student_doc = firestore_db.collection('students').document(student_id).get()
                if student_doc.exists and student_doc.to_dict().get('mac', '').upper() == mac:
                    mac_connected = True

        is_present = face_matched and mac_connected
        timestamp_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        record = {
            "studentId": student_id,
            "name": FACE_CACHE.get(student_id, {}).get("name", "Unknown"),
            "mac": mac,
            "macMatched": mac_connected,
            "faceMatched": face_matched,
            "present": is_present,
            "subject": "General",
            "period": "Face-Verify",
            "dateTime": timestamp_str,
            "photoFilename": photo_filename
        }
        
        records_ref = rtdb.child('records')
        new_key = str(int(time.time() * 1000))
        records_ref.child(new_key).set(record)
        
        # Tell the ESP32 to refresh its Firebase cache instantly!
        try:
            requests.get("http://192.168.4.1/api/sync", timeout=2)
            print("[SYNC] Triggered ESP32 refresh.")
        except Exception as e:
            print(f"[SYNC] ESP32 unreachable: {e}")

        print(f"[ATTENDANCE] {student_id} | Present: {is_present}")
        return jsonify({"ok": True, "present": is_present, "msg": "Success!"})

    except Exception as e:
        print(f"[ATTENDANCE] Error: {e}")
        return jsonify({"ok": False, "msg": "Failed to mark attendance"}), 500

if __name__ == '__main__':
    load_faces_from_firebase()
    print("==================================================")
    print(" FACE RECOGNITION SERVER IS RUNNING")
    print("==================================================")
    app.run(host='0.0.0.0', port=5000, debug=False)
    
