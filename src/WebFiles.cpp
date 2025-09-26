#include "WebFiles.h"

String WebFiles::index_html() {
  // Very small single-file web UI:
  return R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32S3-Zero Setup</title>
  <style>
    body{font-family:Arial; margin:18px;}
    input{padding:8px; margin:6px 0; width:100%;}
    button{padding:10px 16px;margin-top:8px;}
    .card{max-width:520px;}
  </style>
</head>
<body>
  <div class="card">
    <h2>WiFi Setup</h2>
    <form id="wifiForm">
      <label>SSID</label><input id="ssid" name="ssid" required/>
      <label>Password</label><input id="pass" name="pass" type="password"/>
      <button type="submit">Save & Connect</button>
    </form>

    <h2>OTA Upload</h2>
    <form id="otaForm" enctype="multipart/form-data" method="POST" action="/update">
      <input type="file" name="update">
      <button type="submit">Upload firmware (.bin)</button>
    </form>

    <div id="status"></div>
  </div>

<script>
document.getElementById('wifiForm').addEventListener('submit', function(e){
  e.preventDefault();
  var fd = new FormData();
  fd.append('ssid', document.getElementById('ssid').value);
  fd.append('pass', document.getElementById('pass').value);
  fetch('/save', {method:'POST', body:fd}).then(r => r.text()).then(t => {
    document.getElementById('status').innerText = 'Saved. Device will attempt to connect.';
  }).catch(err => {
    document.getElementById('status').innerText = 'Error: ' + err;
  });
});
</script>
</body>
</html>
  )rawliteral";
}
