import firebase_admin
from firebase_admin import credentials, firestore
import pandas as pd

# Path to your service account key
cred = credentials.Certificate(r"C:\Users\devan\OneDrive\Desktop\Mit project file\serviceAccountKey.json")
firebase_admin.initialize_app(cred)

# Connect to Firestore
db = firestore.client()

# Read all documents from the EVData collection
docs = db.collection("EVData").stream()

# Prepare data for Excel
data = []
for doc in docs:
    d = doc.to_dict()
    d['id'] = doc.id  # Add the document ID
    data.append(d)

# Create a DataFrame
df = pd.DataFrame(data)

# Save to Excel file
df.to_excel("EVData.xlsx", index=False)

print("Export completed: EVData.xlsx")
