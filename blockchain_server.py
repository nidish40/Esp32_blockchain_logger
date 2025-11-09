from flask import Flask, request, jsonify, render_template_string
from datetime import datetime

app = Flask(__name__)
blockchain_storage = []

# ---------- Timestamp formatter ----------
def fmt_timestamp(ts):
    try:
        return datetime.fromtimestamp(ts / 1000).strftime("%H:%M:%S")
    except Exception:
        return str(ts)

@app.route('/')
def home():
    return "<h2>ESP32 Blockchain Server</h2><p>POST to <b>/upload_block</b> or view <a href='/blocks'>/blocks</a>.</p>"

# ---------- Upload route ----------
@app.route('/upload_block', methods=['POST'])
def upload_block():
    data = request.get_json(force=True)
    blockchain_storage.append(data)
    print(f"📥 Block #{data.get('index')} uploaded")
    return jsonify({"status": "success"})

# ---------- Pretty HTML Table (auto-refresh) ----------
@app.route('/blocks')
def view_blocks():
    html = """
    <html>
    <head>
      <title>ESP32 Blockchain Dashboard</title>
      <meta http-equiv="refresh" content="10"> <!-- 🔁 Auto-refresh every 10s -->
      <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 40px; background: #f4f4f9; }
        h1 { color: #333; }
        table { border-collapse: collapse; width: 100%; background: white; box-shadow: 0 2px 6px rgba(0,0,0,0.1); }
        th, td { border: 1px solid #ccc; padding: 10px; text-align: center; }
        th { background-color: #2b2b2b; color: #fff; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        footer { margin-top: 20px; text-align: center; font-size: 0.9em; color: #555; }
      </style>
    </head>
    <body>
      <h1>ESP32 Blockchain Data (Auto-Updating)</h1>
      {% if blocks %}
      <table>
        <tr><th>Index</th><th>Distance (cm)</th><th>Prev Hash</th><th>Hash</th><th>Timestamp</th></tr>
        {% for b in blocks %}
          <tr>
            <td>{{ b.index }}</td>
            <td>{{ "%.2f"|format(b.distance) }}</td>
            <td>{{ b.prevHash[:10] }}...</td>
            <td>{{ b.hash[:10] }}...</td>
            <td>{{ fmt(b.timestamp) }}</td>
          </tr>
        {% endfor %}
      </table>
      {% else %}
        <p>No blocks uploaded yet.</p>
      {% endif %}
      <footer>Refreshing every 10 seconds ⟳</footer>
    </body>
    </html>
    """
    return render_template_string(html, blocks=blockchain_storage, fmt=fmt_timestamp)

# ---------- Main ----------
if __name__ == '__main__':
    print("🚀 Flask blockchain server running on port 5000")
    app.run(host='0.0.0.0', port=5000)
