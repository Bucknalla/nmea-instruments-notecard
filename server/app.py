from flask import Flask, request, jsonify, render_template
import logging
import pynmea2
from datetime import datetime

app = Flask(__name__)
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Store the latest parsed data
latest_data = {
    "status": "success",
    "message": "No data received yet",
    "parsed_data": {},
    "location": None,
    "timestamp": None
}

def parse_nmea_message(message):
    try:
        parsed = pynmea2.parse(message)
        return {
            "type": parsed.__class__.__name__,
            "data": parsed.data,
            "raw": str(parsed)
        }
    except pynmea2.ParseError as e:
        logger.error(f"Failed to parse NMEA message: {e}")
        return None

def parse_nmea_section(section_data):
    if not section_data:
        return []

    messages = section_data.strip().split('\n')
    parsed_messages = []

    for message in messages:
        if message.strip():
            parsed = parse_nmea_message(message)
            if parsed:
                parsed_messages.append(parsed)

    return parsed_messages

@app.route('/')
def dashboard():
    return render_template('index.html')

@app.route('/latest_data')
def get_latest_data():
    return jsonify(latest_data)

@app.route('/nmea', methods=['POST'])
def receive_nmea():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"status": "error", "message": "No JSON data received"}), 400

        global latest_data
        latest_data = {
            "status": "success",
            "message": "NMEA data received and parsed",
            "parsed_data": {},
            "timestamp": datetime.now().isoformat()
        }

        # Process HDV section
        if "HDV" in data:
            latest_data["parsed_data"]["HDV"] = parse_nmea_section(data["HDV"])

        # Process INS section
        if "INS" in data:
            latest_data["parsed_data"]["INS"] = parse_nmea_section(data["INS"])

        return jsonify(latest_data), 200
    except Exception as e:
        logger.error(f"Error processing NMEA data: {str(e)}")
        return jsonify({"status": "error", "message": str(e)}), 400

@app.route('/location', methods=['POST'])
def receive_location():
    try:
        data = request.get_json()
        if not data:
            return jsonify({"status": "error", "message": "No JSON data received"}), 400

        # Validate required fields
        required_fields = ['best_location_type', 'best_location_when', 'best_lat', 'best_lon']
        for field in required_fields:
            if field not in data:
                return jsonify({"status": "error", "message": f"Missing required field: {field}"}), 400

        global latest_data
        latest_data["location"] = {
            "type": data["best_location_type"],
            "timestamp": data["best_location_when"],
            "latitude": data["best_lat"],
            "longitude": data["best_lon"]
        }
        latest_data["timestamp"] = datetime.now().isoformat()
        latest_data["status"] = "success"
        latest_data["message"] = "Location data received"

        return jsonify(latest_data), 200
    except Exception as e:
        logger.error(f"Error processing location data: {str(e)}")
        return jsonify({"status": "error", "message": str(e)}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5123, debug=True)
