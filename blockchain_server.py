from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)
blockchain_storage = []

@app.route('/')
def home():
    return """
    <h2>ESP32 Blockchain Server</h2>
    <p>Use <b>/upload_block</b> for POST and <b>/blocks</b> to view all stored blocks.</p>
    """

@app.route('/upload_block', methods=['POST', 'GET'])
def upload_block():
    if request.method == 'POST':
        try:
            data = request.get_json()
            if not data:
                return jsonify({"status": "failed", "reason": "No JSON received"}), 400
            blockchain_storage.append(data)

            index = data.get("index", "?")
            distance = data.get("distance", "?")
            ts = data.get("timestamp", 0)
            try:
                time_str = datetime.fromtimestamp(ts/1000).strftime("%H:%M:%S")
            except Exception:
                time_str = "N/A"

            print(f"📥 Received Block #{index} | Dist: {distance} | Time: {time_str}")
            return jsonify({"status": "success"}), 200
        except Exception as e:
            print("❌ Error receiving block:", e)
            return jsonify({"status": "error", "reason": str(e)}), 500
    else:
        return "<h3>Waiting for ESP32 block uploads...</h3>"

@app.route('/blocks')
def get_blocks():
    return jsonify(blockchain_storage)

if __name__ == '__main__':
    print("🚀 Starting Flask blockchain server...")
    app.run(host='0.0.0.0', port=5000)
