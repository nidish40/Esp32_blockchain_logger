from flask import Flask, request, jsonify, render_template_string
from datetime import datetime

app = Flask(__name__)
blockchain_storage = []

# ---------- Timestamp Formatter ----------
def fmt_timestamp(ts):
    try:
        return datetime.fromtimestamp(ts / 1000).strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(ts)

# ---------- Home Page ----------
@app.route('/')
def home():
    html = """
    <!DOCTYPE html>
    <html lang="en">
    <head>
    <meta charset="UTF-8">
    <title>ESP32 Blockchain Logger</title>
    <style>
      body {
        background-color: #0f1115;
        color: #d6d6d6;
        font-family: 'Segoe UI', sans-serif;
        margin: 0;
        padding: 0;
      }
      header {
        background: linear-gradient(90deg, #2f3640, #353b48);
        color: #00ff99;
        text-align: center;
        padding: 1.5rem 0;
        font-size: 1.8rem;
        font-weight: bold;
        box-shadow: 0 2px 10px rgba(0,0,0,0.5);
      }
      main {
        padding: 2rem;
        max-width: 900px;
        margin: auto;
      }
      footer {
        text-align: center;
        font-size: 0.8rem;
        color: #777;
        padding: 1rem;
        border-top: 1px solid #333;
      }
      a.button {
        background-color: #00ff99;
        color: #111;
        padding: 0.6rem 1rem;
        border-radius: 6px;
        font-weight: bold;
        text-decoration: none;
      }
      a.button:hover {
        background-color: #00cc7a;
      }
    </style>
    </head>
    <body>
      <header>🔗 Blockchain Sensor Data Logger</header>
      <main>
        <h2>Project Overview</h2>
        <p>This system demonstrates a <strong>Blockchain-based Sensor Logger</strong> built using two ESP32 boards connected via the <strong>ESP-NOW protocol</strong>.</p>
        <p>Node A logs ultrasonic sensor data, stores it locally as blockchain blocks, and offloads older blocks to this web server.</p>
        <p>Node B maintains a synchronized copy of the chain in RAM.</p>
        <p><a href="/blocks" class="button">View Stored Blocks</a></p>
      </main>
      <footer>© {{ year }} | ESP32 Blockchain Logger</footer>
    </body>
    </html>
    """
    return render_template_string(html, year=datetime.now().year)

# ---------- Upload Route ----------
@app.route('/upload_block', methods=['POST'])
def upload_block():
    data = request.get_json()
    blockchain_storage.append(data)
    print(f"📥 Block #{data['index']} uploaded")
    return jsonify({"status": "ok"}), 200

# ---------- Blocks Table ----------
@app.route('/blocks')
def view_blocks():
    html = """
    <!DOCTYPE html>
    <html lang="en">
    <head>
    <meta charset="UTF-8">
    <title>Blockchain Blocks</title>
    <meta http-equiv="refresh" content="10"> <!-- Auto-refresh -->
    <style>
      body {
        background-color: #0f1115;
        color: #d6d6d6;
        font-family: 'Segoe UI', sans-serif;
        margin: 0;
        padding: 0;
      }
      header {
        background: linear-gradient(90deg, #2f3640, #353b48);
        color: #00ff99;
        text-align: center;
        padding: 1.5rem 0;
        font-size: 1.8rem;
        font-weight: bold;
        box-shadow: 0 2px 10px rgba(0,0,0,0.5);
      }
      main {
        padding: 2rem;
        max-width: 1000px;
        margin: auto;
      }
      table {
        width: 100%;
        border-collapse: collapse;
        background-color: #1a1c1f;
        color: #e4e4e4;
        border-radius: 8px;
        overflow: hidden;
      }
      th, td {
        padding: 0.75rem;
        border-bottom: 1px solid #333;
        text-align: center;
      }
      th {
        background-color: #22262b;
        color: #00ff99;
      }
      tr:hover {
        background-color: #2a2d33;
      }
      footer {
        text-align: center;
        font-size: 0.8rem;
        color: #777;
        padding: 1rem;
        border-top: 1px solid #333;
      }
    </style>
    </head>
    <body>
      <header>🧱 Blockchain Data Blocks</header>
      <main>
        {% if blocks %}
        <table>
          <tr>
            <th>Index</th>
            <th>Distance (cm)</th>
            <th>Prev Hash</th>
            <th>Hash</th>
            <th>Timestamp</th>
          </tr>
          {% for b in blocks %}
          <tr>
            <td>{{ b.index }}</td>
            <td>{{ "%.2f"|format(b.distance) }}</td>
            <td>{{ b.prevHash[:8] }}</td>
            <td>{{ b.hash[:8] }}</td>
            <td>{{ fmt(b.timestamp) }}</td>
          </tr>
          {% endfor %}
        </table>
        {% else %}
        <p>No blocks uploaded yet.</p>
        {% endif %}
      </main>
      <footer>Auto-refreshes every 10 seconds | © {{ year }}</footer>
    </body>
    </html>
    """
    return render_template_string(html, blocks=blockchain_storage, fmt=fmt_timestamp, year=datetime.now().year)

if __name__ == '__main__':
    print("🚀 Flask blockchain server running on port 5000")
    app.run(host='0.0.0.0', port=5000)
