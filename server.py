from flask import Flask, render_template_string, request, jsonify
import requests


app = Flask(__name__)


# List of ESP32 devices with their IP addresses
esp32_devices = [
   {'ip': '198.51.100.10'},
   {'ip': '203.0.113.45'}
]


ESP32_PORT = 80


# Global list to store received messages
messages = []


# HTML template for the dashboard
html_template = '''
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>FleetLink Messaging Dashboard</title>
</head>
<body>
  <div style="text-align: center; margin-top: 50px;">
    <h1>FleetLink Messaging Dashboard</h1>
    <form method="POST" action="/">
      <input type="text" name="message" placeholder="Enter your message" style="width: 300px; height: 30px; font-size: 16px;">
      <br><br>
      <input type="submit" value="Send to All Ships" style="width: 205px; height: 40px; font-size: 16px;">
    </form>
    {% if feedback %}
      <p style="color: green; font-size: 18px;">{{ feedback }}</p>
    {% endif %}
    {% if error %}
      <p style="color: red; font-size: 18px;">{{ error }}</p>
    {% endif %}


    <h2>Received Messages from Ships</h2>
    <ul id="messages">
      {% for msg in messages %}
        <li>{{ msg }}</li>
      {% endfor %}
    </ul>


    <script>
      function fetchMessages() {
        fetch('/get-messages')
          .then(response => response.json())
          .then(data => {
            const messagesList = document.getElementById('messages');
            messagesList.innerHTML = '';
            data.forEach(msg => {
              const listItem = document.createElement('li');
              listItem.textContent = msg;
              messagesList.appendChild(listItem);
            });
          })
          .catch(error => console.error('Error fetching messages:', error));
      }


      // Fetch messages every 5 seconds
      setInterval(fetchMessages, 5000);
    </script>
  </div>
</body>
</html>
'''


@app.route('/', methods=['GET', 'POST'])
def index():
   feedback = ''
   error = ''
   message = ''
   if request.method == 'POST':
       message = request.form.get('message')
       if message:
           # Send the message to all ESP32 devices via HTTP POST
           for device in esp32_devices:
               try:
                   url = f'http://{device["ip"]}:{ESP32_PORT}/receive-message'
                   payload = {'message': message}
                   response = requests.post(url, data=payload, timeout=5)
                   if response.status_code == 200:
                       feedback = f"Message '{message}' sent to Ships."
                   else:
                       error = f"Failed to send message to {device['ip']}. ESP32 responded with status code {response.status_code}."
                       break
               except requests.exceptions.RequestException as e:
                   error = f"Error communicating with ESP32 at {device['ip']}: {e}"
                   break
       else:
           error = "Message is empty."
   return render_template_string(html_template, feedback=feedback, error=error, messages=messages)


@app.route('/receive-message', methods=['POST'])
def receive_message():
   message = request.form.get('message')
   sender = request.form.get('sender', 'Unknown')
   ship = request.form.get('ship', 'Unknown Ship')
   if message:
       messages.append(f"From {ship} ({sender}): {message}")
       return 'Message received', 200
   else:
       return 'No message received', 400


@app.route('/get-messages', methods=['GET'])
def get_messages():
   return jsonify(messages)


if __name__ == '__main__':
   app.run(debug=True, host='0.0.0.0')
