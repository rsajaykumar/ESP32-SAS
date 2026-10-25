import firebase_admin
from firebase_admin import credentials, firestore
import face_recognition
import sys, os
from datetime import datetime

cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred)
db = firestore.client()

def enroll(student_id, name, mac, image_path):
    if not os.path.exists(image_path):
        print(f"❌ Image not found: {image_path}")
        return

    img = face_recognition.load_image_file(image_path)
    encodings = face_recognition.face_encodings(img)

    if not encodings:
        print("❌ No face detected in the photo.")
        return
    
    encoding = encodings[0].tolist()
    mac = mac.strip().upper()
    
    db.collection("students").document(student_id).set({
        "id": student_id,
        "name": name,
        "mac": mac,
        "faceEncoding": encoding,
        "registeredAt": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    }, merge=True)
    print(f"✅ Enrolled: {student_id} | {name} | {mac}")

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python enroll.py <studentId> <name> <mac> <imagePath>")
        sys.exit(1)
    enroll(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
