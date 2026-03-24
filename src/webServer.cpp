#include "webServer.h"
#include "rtc.h"

AsyncWebServer server(80);
bool serverData = false;
String ssid = "";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Meter Web Server</title>
    <link rel="stylesheet" href="/styles.css">
</head>

<body>

    <div class="header-bar">
        <h1 style="margin: 0; font-size: 1.5em;">Smart Meter Control Panel</h1>
        <div style="display: flex; align-items: center; gap: 15px;">
            <div style="font-size: 0.9em; color: #666;">Current Time:</div>
            <div id="headerTime" style="font-size: 1.1em; font-weight: bold; color: #333;">Loading...</div>
        </div>
    </div>


    <div class="tab-container">

        <div class="tab-header">
            <button onclick="openTab('tab1')">WiFi Configuration</button>
            <button onclick="openTab('tab2')">Channel Checker</button>
            <button onclick="openTab('tab3')">Slot Configuration</button>
            <button onclick="openTab('tab7')">SD Card Files</button>
        </div>
                <!-- SD Card Files Tab -->
        <div id="tab7" class="tab-content">
            <h2>SD Card File Browser</h2>
            <div class="file-controls">
                <button onclick="refreshFileList()" class="btn-primary">Refresh Files</button>
                <div class="file-stats">
                    <span>Total Files: <strong id="file-count">0</strong></span>
                    <span style="margin-left: 20px;">Total Size: <strong id="total-size">0 KB</strong></span>
                </div>
            </div>
            <div class="date-range-box" style="margin: 20px 0; padding: 15px; border: 1px solid #ccc; border-radius: 10px;">
        <h3>Download Files by Date Range</h3>

        <label>Start Date:</label>
        <input type="date" id="start-date" style="margin-right: 20px;">

        <label>End Date:</label>
        <input type="date" id="end-date" style="margin-right: 20px;">

        <button type="button" onclick="downloadRange()" class="btn-primary">
    Download Range
</button>

         </div>            
            <div id="file-list-container">
                <table id="file-table">
                    <thead>
                        <tr>
                            <th>File Name</th>
                            <th>Size</th>
                            <th>Type</th>
                            <th>Action</th>
                        </tr>
                    </thead>
                    <tbody id="file-list">
                        <tr><td colspan="4" class="loading">Loading files...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>




        <div id="tab1" class="tab-content">
            <h2>WiFi Configuration</h2>
             <form action="/wifi" method="post">
                <div class="form-group">
                    <label for="HTML_SSID">SSID:</label>
                    <input type="text" id="HTML_SSID" name="HTML_SSID" value="" required>
                </div>
                <div class="form-group">
                    <label for="HTML_PASS">Password:</label>
                    <input type="password" id="HTML_PASS" name="HTML_PASS" value="" required>
                </div>
                <div class="form-group">
                    <button type="submit">Connect</button>
                </div>
            </form>
        </div>

        <div id="tab2" class="tab-content">
            <h2>Channel Checker
            <sub style="font-size: 12px; color: gray;" id="fw-version"></sub></h2>


    <div class="status-container">
    <div id="wifi-card" class="status-card checking">
        <div class="status-label">Wi-Fi</div>
        <div class="status-value" id="wifi-status">Checking...</div>
        <div class="status-icon"></div>
    </div>
    
    <div id="storage-card" class="status-card checking">
        <div class="status-label">Storage</div>
        <div class="status-value" id="storage-status">Checking...</div>
        <div class="status-icon"></div>
    </div>
    
    <div id="rtc-card" class="status-card checking">
        <div class="status-label">RTC</div>
        <div class="status-value" id="rtc-status">Checking...</div>
        <div class="status-icon"></div>
    </div>
</div>
            <table id="dataTable">
                <thead>
                    <tr>
                        <th>No. #</th>
                        <th>Slot Labels</th>
                        <th>Voltage (V)</th>
                        <th>Current (A) </th>
                        <th>Power (W)</th>
                        <th>Power Factor</th>
                    </tr>
                </thead>
                <tbody></tbody>
            </table>
            <button onclick="restartESP()" style="background-color:rgb(95, 226, 198); color: black; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer;">
                Restart ESP32
            </button>

        </div>

<div id="tab3" class="tab-content">
    <h2>Slot Configuration</h2>
    <form id="slotConfigForm" action="/slots" method="post">
    </form>
    <div class="form-group" style="margin-top: 30px; border-top: 2px solid #ccc; padding-top: 20px;">
        <h3 style="color: #d9534f;">Caution</h3>
        <button type="button" onclick="resetEnergy()" style="background-color: #d9534f; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; width: 100%;">
            Reset Energy Counters (All Slots)
        </button>
        <div id="reset_energy_status" class="text-center mt-4"></div>
    </div>
</div>

</div>

    <script src="index.js"></script>
</body>

</html>
)rawliteral";

const char styles_css[] PROGMEM = R"rawliteral(
body {
    font-family: Arial, sans-serif;
    font-size: 16px;
    background-color: #faf9f6;
    margin: 0;
}
    .header-bar {
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    padding: 15px 30px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}

.header-bar h1 {
    color: white;
}

.header-bar #headerTime {
    background: rgba(255,255,255,0.2);
    padding: 8px 15px;
    border-radius: 5px;
    color: white !important;
    font-family: 'Courier New', monospace;
}

.tab-container {
    width: 100%;
    margin: 0 auto;
}

.tab-header {
    overflow: hidden;
    border: 1px solid #ccc;
}

.tab-header button {
    background-color: inherit;
    color: black;
    float: left;
    border: none;
    cursor: pointer;
    padding: 1em;
    font-size: 1em;
    transition: 0.3s;
    font-weight: bold;
}

.tab-content {
    display: none;
    padding: 1em;
    border: 1px solid #ccc;
}

table {
    width: 100%;
}

th,
td {
    border: 1px solid #dddddd;
    text-align: center;
    padding: 0.5em 0;
}

th {
    font-weight: bold;
    background-color:rgb(95, 226, 198);
}

.form-group {
    display: flex;
    align-items: center;
    margin-bottom: 1em;
}

.form-group label {
    flex: 1;
    font-weight: bold;
    margin-right: 1em;
    color: black;
}

.form-group select {
    flex: 2;
    margin-right: 1em;
    padding: 1em;
    border: 1px solid #ccc;
    border-radius: 10px;
    box-sizing: border-box;
}

.form-group input {
    flex: 6;
    margin-right: 1em;
    padding: 1em;
    border: 1px solid #ccc;
    border-radius: 20px;
    box-sizing: border-box;
}

.form-group button {
    background-color: #4CAF50;
    color: white;
    padding: 1em;
    border: none;
    font-size: 1em;
    border-radius: 20px;
    cursor: pointer;
    transition: background-color 0.3s ease;
}

.form-group button:hover {
    background-color: #45a049;
}

h2 {
    text-align: center;
    margin-bottom: 30px;
    color: black;
}

.container:before,
.container:after {
    content: "";
    display: table;
    clear: both;
}

.container:after {
    clear: both;
}

.form-group button[type="submit"] {
    margin-left: 25%;
    margin-right: 25%;
    width: 100%;
}
    .calibration-types {
    display: flex;
    justify-content: center;
    margin-bottom: 20px;
}

.calibration-tab {
    background-color: #f2f2f2;
    border: 1px solid #ccc;
    padding: 10px 20px;
    cursor: pointer;
    transition: background-color 0.3s;
    font-weight: bold;
}

.calibration-tab.active {
    background-color: #4CAF50;
    color: white;
}

.calibration-section {
    margin-top: 20px;
}

.slot-checkbox {
    margin-right: 10px;
}
.slot-header {
    background: #f2f2f2;
    font-weight: bold;
    text-align: left;
}

h3 {
    text-align: center;
    margin-bottom: 20px;
    color: black;
}

.slot-calibration-row {
    display: flex;
    align-items: center;
    margin-bottom: 15px;
    padding: 10px;
    border: 1px solid #ddd;
    border-radius: 5px;
    background-color: #f9f9f9;
}

.slot-checkbox-container {
    flex: 1;
    display: flex;
    align-items: center;
}

.slot-calibration-input {
    flex: 2;
    padding: 8px;
    border: 1px solid #ccc;
    border-radius: 4px;
}

.slot-current-value {
    flex: 1;
    text-align: center;
    font-weight: bold;
}
.status-container {
    display: flex;
    gap: 4px; /* Minimal gap */
    margin-bottom: 8px; /* Minimal bottom margin */
    justify-content: center;
    flex-wrap: wrap;
}

.status-card {
    background: white;
    border-radius: 3px; /* Very minimal rounding */
    padding: 4px 6px; /* Ultra compact padding */
    box-shadow: 0 1px 2px rgba(0,0,0,0.05); /* Very subtle shadow */
    border-left: 2px solid #ccc;
    min-width: 85px; /* Much smaller width */
    max-width: 30%; /* Also use relative width for responsiveness */
    transition: all 0.3s ease;
    flex: 1; /* Allow flexible growth */
}

/* Status Card States */
.status-card.connected {
    border-left-color:rgb(95, 226, 198);
    background-color: #f8fff9;
}

.status-card.disconnected {
    border-left-color: #dc3545;
    background-color: #fff8f8;
}

.status-card.checking {
    border-left-color: #ffc107;
    background-color: #fffdf7;
}

.status-label {
    font-weight: bold;
    font-size: 9px; /* Extremely small */
    color: #666;
    margin-bottom: 1px; /* Minimal spacing */
    text-transform: uppercase;
    letter-spacing: 0.3px;
    white-space: nowrap; /* Prevent wrapping */
}

.status-value {
    font-size: 11px; /* Very small */
    font-weight: 600;
    white-space: nowrap; /* Prevent wrapping */
    overflow: hidden;
    text-overflow: ellipsis; /* Show ellipsis for overflow */
}

/* Status Value Colors */
.status-card.connected .status-value {
    color:rgb(95, 226, 198);
}

.status-card.disconnected .status-value {
    color: #dc3545;
}

.status-card.checking .status-value {
    color: #ffc107;
}

.status-icon {
    float: right;
    font-size: 10px; /* Tiny icon */
    margin-left: 3px;
    margin-top: 0px;
}

/* Media query for extra small screens */
@media (max-width: 360px) {
    .status-container {
        gap: 3px;
    }
    
    .status-card {
        min-width: 75px;
    }
    
    .status-label {
        font-size: 8px;
    }
    
    .status-value {
        font-size: 10px;
    }
}
.status-card.connected .status-icon::before {
    content: "✓";
    color:rgb(95, 226, 198);
}

.status-card.disconnected .status-icon::before {
    content: "✗";
    color: #dc3545;
}

.status-card.checking .status-icon::before {
    content: "⏳";
}
    .status-card.limited {
    border-left-color: #fd7e14; /* Orange-ish */
    background-color: #fff8f2;
}

.status-card.limited .status-value {
    color: #fd7e14;
}

.status-card.limited .status-icon::before {
    content: "⚠️";
    color: #fd7e14;
}
    /* Time Display Styles */
.time-display-container {
    margin-bottom: 30px;
}

.time-display {
    font-size: 48px;
    font-weight: bold;
    color: #2196F3;
    border: 3px solid #2196F3;
    padding: 20px 30px;
    border-radius: 12px;
    display: inline-block;
    background-color: #f8f9fa;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    font-family: 'Courier New', monospace;
    min-width: 400px;
}

.last-updated {
    margin-top: 15px;
    color: #666;
    font-size: 14px;
    font-style: italic;
}

.refresh-btn {
    padding: 12px 24px;
    font-size: 16px;
    background-color: #4CAF50;
    color: white;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    transition: background-color 0.3s;
}

.refresh-btn:hover {
    background-color: #45a049;
}

/* Password Modal Styles */
.password-modal {
    display: none;
    position: fixed;
    z-index: 1000;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    background-color: rgba(0, 0, 0, 0.5);
}

.password-modal-content {
    background-color: #faf9f6;
    margin: 15% auto;
    padding: 20px;
    border: 1px solid #888;
    border-radius: 10px;
    width: 300px;
    text-align: center;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
}

.password-modal h3 {
    margin-top: 0;
    color: #333;
}

.password-modal input {
    width: 100%;
    padding: 10px;
    margin: 10px 0;
    border: 1px solid #ccc;
    border-radius: 5px;
    box-sizing: border-box;
}

.password-modal button {
    background-color: #4CAF50;
    color: white;
    padding: 10px 20px;
    margin: 5px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
}

.password-modal button:hover {
    background-color: #45a049;
}

.password-modal .cancel-btn {
    background-color: #f44336;
}

.password-modal .cancel-btn:hover {
    background-color: #da190b;
}

)rawliteral";

const char index_js[] PROGMEM = R"rawliteral(
const numOfSlots = )rawliteral" __STR(NUM_OF_SLOTS) R"rawliteral(;
sensorDataInterval = undefined;

// Password protection variables
let isAuthenticated = false;
let protectedTabs = ['tab3']; //Slot Configuration

// Create and show password modal
function showPasswordModal(tabName) {
    let modal = document.getElementById('passwordModal');
    if (!modal) {
        modal = document.createElement('div');
        modal.id = 'passwordModal';
        modal.className = 'password-modal';
        modal.innerHTML = `
            <div class="password-modal-content">
                <h3>Admin Access Required</h3>
                <p>Please enter the admin password to access this section:</p>
                <input type="password" id="passwordInput" placeholder="Enter password">
                <div id="passwordError" style="color: red; margin: 10px 0;"></div>
                <button onclick="verifyPassword('${tabName}')">Submit</button>
                <button class="cancel-btn" onclick="closePasswordModal()">Cancel</button>
            </div>
        `;
        document.body.appendChild(modal);
    }
    
    // Clear previous inputs and show modal
    document.getElementById('passwordInput').value = '';
    document.getElementById('passwordError').textContent = '';
    modal.style.display = 'block';
    document.getElementById('passwordInput').focus();
}

// Close password modal
function closePasswordModal() {
    const modal = document.getElementById('passwordModal');
    if (modal) {
        modal.style.display = 'none';
    }
}

// Verify password with server
function verifyPassword(tabName) {
    const password = document.getElementById('passwordInput').value;
    const errorDiv = document.getElementById('passwordError');
    
    if (!password) {
        errorDiv.textContent = 'Please enter a password';
        return;
    }
    
    const formData = new FormData();
    formData.append('password', password);
    
    fetch('/verify-password', {
        method: 'POST',
        body: formData
    })
    .then(response => response.text())
    .then(data => {
        if (data === 'success') {
            isAuthenticated = true;
            closePasswordModal();
            openTab(tabName);
        } else {
            errorDiv.textContent = 'Invalid password';
            document.getElementById('passwordInput').value = '';
        }
    })
    .catch(error => {
        errorDiv.textContent = 'Error verifying password';
        console.error('Error:', error);
    });
}

// Handle Enter key in password input
document.addEventListener('keydown', function(event) {
    if (event.key === 'Enter' && document.getElementById('passwordModal') && 
        document.getElementById('passwordModal').style.display === 'block') {
        const activeTab = document.querySelector('.password-modal-content button[onclick*="tab"]');
        if (activeTab) {
            const tabName = activeTab.getAttribute('onclick').match(/verifyPassword\('(\w+)'\)/)[1];
            verifyPassword(tabName);
        }
    }
});

function openTab(tabName) {
    if (protectedTabs.includes(tabName) && !isAuthenticated) {
        showPasswordModal(tabName);
        return;
    }

    var tabContent = document.getElementsByClassName("tab-content");
    for (var i = 0; i < tabContent.length; i++) {
        tabContent[i].style.display = "none";
    }
    document.getElementById(tabName).style.display = "block";

    clearInterval(sensorDataInterval);

    if (tabName == "tab2") {
        updateSensorData();
        updateSlotInfo();
        updateversion();
        sensorDataInterval = setInterval(updateSensorData, 1000);
    }

    if (tabName == "tab3") {
        updateSlotInfo();
    }
    if(tabName == "tab4") {
        document.getElementById("calibration_status").textContent = "";
    }
    if(tabName == "tab5") {
        document.getElementById("current_status").textContent = "";
    }
}
function updateversion()
{
fetch('/firmware')
  .then(response => response.text())
  .then(version => {
    document.getElementById('fw-version').innerText = " v" + version;
  });
}

// Update header time periodically
function updateHeaderTime() {
    fetch('/get-time')
        .then(response => response.text())
        .then(time => {
            document.getElementById('headerTime').textContent = time;
        })
        .catch(error => {
            document.getElementById('headerTime').textContent = 'Error loading time';
        });
}

// Initialize header time and set interval
updateHeaderTime();
setInterval(updateHeaderTime, 30000); // Update every 30 seconds


function generateTableRows() {
    var tableBody = document.getElementById("dataTable").getElementsByTagName('tbody')[0];

    for (var i = 1; i <= numOfSlots; i++) {
        tableBody.innerHTML += '<tr><td>' + i + '</td><td id="slot-label-' + i + '">--</td><td id="V' + i + '">--</td><td id="I' + i + '">--</td><td id="P' + i + '">--</td><td id="PF' + i + '">--</td></tr>';
    }
}
function generateSlotConfigRows() {
    var slotConfigForm = document.getElementById("slotConfigForm");

    for (var i = 1; i <= numOfSlots; i++) {
        slotConfigForm.innerHTML += `
            <div class="form-group">
                <label>S${i}</label>
                <select id="slot-factor-${i}" name="slot-factor-${i}" onchange="toggleCustomInput(${i})">
                    <option value="0.30">30 A </option>
                    <option value="0.60">60 A </option>
                    <option value="1.00">100 A </option>
                    <option value="2.00">200 A </option>
                    <option value="3.00">300 A </option>
                    <option value="4.00">400 A </option>
                    <option value="5.00">500 A </option>
                    <option value="6.00">600 A </option>
                    <option value="10.00">1000 A </option>
                    <option value="custom">Custom</option>
                </select>
                <input type="number" step="0.01" id="custom-factor-${i}" name="slot-factor-${i}" 
                       style="display:none; flex:1; width:100px;" placeholder="Custom">
                <input type="text" id="slot-label-input-${i}" name="slot-label-${i}" maxlength="100" placeholder="Enter S${i} label">
            </div>
        `;
    }

    slotConfigForm.innerHTML += `
        <div class="form-group">
            <button type="submit">Update</button>
        </div>
    `;
}

// Add this function to toggle custom input visibility
function toggleCustomInput(slotNumber) {
    var selectElement = document.getElementById(`slot-factor-${slotNumber}`);
    var customInput = document.getElementById(`custom-factor-${slotNumber}`);
    
    if (selectElement.value === 'custom') {
        customInput.style.display = 'block';
        customInput.required = true;
    } else {
        customInput.style.display = 'none';
        customInput.required = false;
    }
}

function updateSensorData() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200) {
            var sensorData = JSON.parse(xhr.responseText);

            for (var i = 1; i <= numOfSlots; i++) {
                document.getElementById('V' + i).textContent = sensorData['V' + i];
                document.getElementById('I' + i).textContent = sensorData['I' + i];
                document.getElementById('P' + i).textContent = sensorData['P' + i];
                document.getElementById('PF' + i).textContent = sensorData['PF' + i];
            }
        }
    };
    xhr.open("GET", "/data", true);
    xhr.send();
}

function resetEnergy() {
    if (confirm('Are you sure you want to reset energy counters for all slots? This action cannot be undone!')) {
        document.getElementById("reset_energy_status").textContent = "Resetting energy...";
        
        fetch('/reset-energy', {
            method: 'POST'
        })
        .then(response => response.text())
        .then(message => {
            document.getElementById("reset_energy_status").textContent = message;
            // Clear the message after 5 seconds
            setTimeout(() => {
                document.getElementById("reset_energy_status").textContent = "";
            }, 5000);
        })
        .catch(error => {
            document.getElementById("reset_energy_status").textContent = 
                "Error resetting energy: " + error.message;
        });
    }
}

function updateSlotInfo() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200) {
            console.log(xhr.responseText)
            var slotInfo = JSON.parse(xhr.responseText);

            for (var i = 1; i <= numOfSlots; i++) {
                // Update labels in Channel Checker tab
                const labelElement = document.getElementById('slot-label-' + i);
                if (labelElement) {
                    labelElement.textContent = slotInfo['slot-label-' + i];
                }

                // Update input fields in Slot Configuration tab
                const inputElement = document.getElementById('slot-label-input-' + i);
                if (inputElement) {
                    inputElement.value = slotInfo['slot-label-' + i];
                }

                // Update select dropdowns and handle custom values
                var selectElement = document.getElementById('slot-factor-' + i);
                var customInput = document.getElementById('custom-factor-' + i);
                
                if (selectElement && customInput) {
                    var storedFactor = parseFloat(slotInfo['slot-factor-' + i]);
                    var isPredefinedOption = Array.from(selectElement.options)
                        .some(option => parseFloat(option.value) === storedFactor);

                    if (!isPredefinedOption && storedFactor !== 0) {
                        // This is a custom value
                        selectElement.value = 'custom';
                        customInput.style.display = 'block';
                        customInput.value = storedFactor.toString();
                    } else {
                        // Set to predefined option
                        for (var j = 0; j < selectElement.options.length; j++) {
                            if (selectElement.options[j].value == storedFactor) {
                                selectElement.selectedIndex = j;
                                customInput.style.display = 'none';
                                break;
                            }
                        }
                    }
                }
            }
        }
    };
    xhr.open("GET", "/slots", true);
    xhr.send();
}




function refreshFileList() {
    const fileList = document.getElementById('file-list');
    const fileCountEl = document.getElementById('file-count');
    const totalSizeEl = document.getElementById('total-size');

    fileList.innerHTML =
        '<tr><td colspan="4" class="loading">Loading files...</td></tr>';

    fetch('/list-files')
        .then(res => res.json())
        .then(data => {
            fileList.innerHTML = '';

            let totalFiles = 0;
            const fragment = document.createDocumentFragment();

            // ---------- Helper: Create table row ----------
            const createRow = (file, pathPrefix = '') => {
                const tr = document.createElement('tr');
                tr.innerHTML = `
                    <td>${pathPrefix}${file.name}</td>
                    <td>${(file.size / 1024).toFixed(2)} KB</td>
                    <td>${file.type}</td>
                    <td>
                        <button class="download-btn"
                            onclick="downloadFile('${pathPrefix}${file.name}')">
                            Download
                        </button>
                    </td>
                `;
                return tr;
            };

            // ---------- ROOT FILES (APs.txt etc) ----------
            if (Array.isArray(data.rootFiles)) {
                data.rootFiles.forEach(file => {
                    fragment.appendChild(createRow(file));
                    totalFiles++;
                });
            }

            // ---------- SLOT FILES ----------
            if (Array.isArray(data.slots)) {
                data.slots.forEach(slot => {

                    // Slot header row
                    const headerRow = document.createElement('tr');
                    headerRow.innerHTML = `
                        <td colspan="4" class="slot-header">
                            ${slot.slot.toUpperCase()}
                        </td>
                    `;
                    fragment.appendChild(headerRow);

                    if (Array.isArray(slot.files)) {
                        slot.files.forEach(file => {
                            fragment.appendChild(
                                createRow(file, `${slot.slot}/`)
                            );
                            totalFiles++;
                        });
                    }
                });
            }

            if (totalFiles === 0) {
                fileList.innerHTML =
                    '<tr><td colspan="4" class="loading">No files found on SD card</td></tr>';
                fileCountEl.textContent = '0';
                totalSizeEl.textContent = '0 KB';
                return;
            }

            fileList.appendChild(fragment);
            fileCountEl.textContent = totalFiles;
            totalSizeEl.textContent =
                (data.totalSize / 1024).toFixed(2) + ' KB';
        })
        .catch(err => {
            console.error('Error fetching files:', err);
            fileList.innerHTML =
                '<tr><td colspan="4" class="loading">Error loading files</td></tr>';
            fileCountEl.textContent = '0';
            totalSizeEl.textContent = '0 KB';
        });
}


// Download file
function downloadFile(fileName) {
    window.location.href = '/download?file=' + encodeURIComponent(fileName);
}

function downloadRange() {
    const s = document.getElementById("start-date").value.replace(/-/g, '');
    const e = document.getElementById("end-date").value.replace(/-/g, '');
    window.location.href =
        `/download-range-json?start=${s}&end=${e}`;
}



function restartESP() {
  if (confirm('Are you sure you want to restart the ESP32?')) {
    fetch('/restart', { method: 'POST' })
    .then(response => {
      if (response.ok) {
        alert('ESP32 is restarting...');
        setTimeout(() => {
          window.location.reload();
        }, 5000);
      }
    })
    .catch(error => {
      // This is expected when ESP32 restarts and drops connection
      alert('ESP32 restart command sent. Device is restarting...');
      setTimeout(() => {
        window.location.reload();
      }, 5000);
    });
  }
}

function getTime() {
      fetch('/get-time')
        .then(response => response.text())
        .then(data => {
          document.getElementById('timeDisplay').innerText = data;
        })
        .catch(error => {
          console.error('Error:', error);
        });
    }

function updateStatusCard(cardId, label, value) {
    const card = document.getElementById(cardId);
    const valueElement = card.querySelector('.status-value');
    const iconElement = card.querySelector('.status-icon');

    // Set the status text
    valueElement.textContent = value;

    // Remove all possible status classes
    card.classList.remove('connected', 'disconnected', 'checking', 'limited');
    iconElement.className = 'status-icon'; // reset base

    // Apply correct status class based on value
    if (value.includes("Failed") || value === "Disconnected!") {
        card.classList.add('disconnected');
    } else if (value === "Limited!") {
        card.classList.add('limited');
    } else if (value === "Checking..." || value === "Error fetching") {
        card.classList.add('checking');
    } else {
        card.classList.add('connected');
    }
}


        function fetchStatus() {
            fetch('/getStatus')
                .then(response => response.json())
                .then(data => {
                    updateStatusCard('wifi-card', 'wifi', data.wifi);
                    updateStatusCard('storage-card', 'storage', data.storage);
                    updateStatusCard('rtc-card', 'rtc', data.rtc);
                })
                .catch(error => {
                    updateStatusCard('wifi-card', 'wifi', 'Error fetching');
                    updateStatusCard('storage-card', 'storage', 'Error fetching');
                    updateStatusCard('rtc-card', 'rtc', 'Error fetching');
                });
        }

        // Simulate status updates for demo
        function simulateStatusUpdates() {
            setTimeout(() => {
                updateStatusCard('wifi-card', 'wifi', 'Connected to "NeuBolt"');
                updateStatusCard('storage-card', 'storage', 'Storage is working');
                updateStatusCard('rtc-card', 'rtc', 'RTC is working');
            }, 2000);
        }

        // Call fetchStatus once page loads
        window.onload = function() {
            fetchStatus();
            setInterval(fetchStatus, 3000);
            
            // For demo purposes - remove this in production
            simulateStatusUpdates();
        };


document.addEventListener('submit', function(e) {
    if (e.target.id === 'slotConfigForm') {
        for (var i = 1; i <= numOfSlots; i++) {
            var selectElement = document.getElementById(`slot-factor-${i}`);
            var customInput = document.getElementById(`custom-factor-${i}`);
            
            if (selectElement.value === 'custom') {   
                var inputValue = parseFloat(customInput.value);
                var processedValue;


                if (inputValue >= 1000) {
                    processedValue = (inputValue / 100).toFixed(2);
                } else {
                    processedValue = inputValue.toFixed(2);
                }


                    selectElement.value = processedValue.toString();
                    customInput.value = processedValue.toString();
                
            }
        }
    }
});
document.addEventListener("DOMContentLoaded", function () {
    // Your existing functions
    generateTableRows();
    generateSlotConfigRows();
    openTab('tab1');
});





)rawliteral";

String getFileType(const String &name)
{
    if (name.endsWith(".txt"))
        return "Text";
    if (name.endsWith(".csv"))
        return "CSV";
    if (name.endsWith(".log"))
        return "Log";
    return "Data";
}

String meterDataAsJson()
{
    String json = "{";
    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
        json += "\"V" + String(i + 1) + "\":\"" + String(meterData[i].getInstVoltage(), 2) + "\",";
        json += "\"I" + String(i + 1) + "\":\"" + String(meterData[i].getInstCurrent(), 2) + "\",";
        json += "\"P" + String(i + 1) + "\":\"" + String(meterData[i].getInstPower(), 2) + "\",";

        if (i == NUM_OF_SLOTS - 1)
        {
            json += "\"PF" + String(i + 1) + "\":\"" + String(meterData[i].getInstPf(), 2) + "\"";
        }
        else
        {
            json += "\"PF" + String(i + 1) + "\":\"" + String(meterData[i].getInstPf(), 2) + "\",";
        }
    }
    json += "}";
    log_d("%s", json.c_str());
    return json;
}

String slotLabelsAsJson()
{
    String slot_labels[NUM_OF_SLOTS];
    float slot_factors[NUM_OF_SLOTS];
    myNVS::read::slotLabels(slot_labels);
    myNVS::read::slotFactors(slot_factors);
    String json = "{";
    for (int i = 0; i < NUM_OF_SLOTS; i++)
    {
        if (i == NUM_OF_SLOTS - 1)
        {
            json += "\"slot-label-" + String(i + 1) + "\":\"" + slot_labels[i] + "\",";
            json += "\"slot-factor-" + String(i + 1) + "\":\"" + slot_factors[i] + "\"";
        }
        else
        {
            json += "\"slot-label-" + String(i + 1) + "\":\"" + slot_labels[i] + "\",";
            json += "\"slot-factor-" + String(i + 1) + "\":\"" + slot_factors[i] + "\",";
        }
    }
    json += "}";
    log_d("%s", json.c_str());
    return json;
}

String templateProcessor(const String &var)
{
    if (var == "NUM_OF_SLOTS")
    {
        return String(NUM_OF_SLOTS);
    }
    return String();
}

void handleDownloadRangeJSON(AsyncWebServerRequest *request)
{
    if (!request->hasParam("start") || !request->hasParam("end"))
    {
        request->send(400, "text/plain", "Start or End date missing");
        return;
    }

    String start = request->getParam("start")->value();
    String end = request->getParam("end")->value();

    // Prepare Async stream response
    AsyncResponseStream *res =
        request->beginResponseStream("application/json");

    String fname = "logs_" + start + "_" + end + ".json";
    res->addHeader(
        "Content-Disposition",
        "attachment; filename=\"" + fname + "\"");

    if (!SD.begin())
    {
        res->print("{\"error\":\"SD card not found\"}");
        request->send(res);
        return;
    }

    File dir = SD.open("/slot1"); // change slot folder as needed
    if (!dir)
    {
        res->print("{\"error\":\"Slot folder not found\"}");
        request->send(res);
        return;
    }

    StaticJsonDocument<8192> doc; // adjust size based on expected data
    File f;
    uint8_t buf[512];

    while ((f = dir.openNextFile()))
    {
        if (f.isDirectory())
            continue;

        String name = f.name();
        if (!name.endsWith(".txt") || name.length() != 12)
            continue;

        String date = name.substring(0, 8);
        if (date < start || date > end)
            continue;

        // Read file content into a String
        String content;
        while (f.available())
        {
            size_t r = f.read(buf, sizeof(buf));
            content += String((char *)buf).substring(0, r);
        }

        // Add file content to JSON object
        doc[name] = content;

        f.close();
    }

    // Serialize JSON to the response stream
    if (serializeJson(doc, *res) == 0)
    {
        res->print("{\"error\":\"Failed to serialize JSON\"}");
    }

    request->send(res);
}

void myServerInitialize()
{
    // end server if already on
    server.end();

    // serve html
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", index_html); });

    // serve css
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/css", reinterpret_cast<PGM_P>(styles_css)); });

    // serve js
    server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/javascript", index_js); });

    // instantaneous data for channel checker
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", meterDataAsJson()); });

    // retrieve wifi credentials from web page
    server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("HTML_SSID", true))
    {
        serverWifiCreds[0] = request->getParam("HTML_SSID", true)->value();
    }

    if (request->hasParam("HTML_PASS", true))
    {
        serverWifiCreds[1] = request->getParam("HTML_PASS", true)->value();
    }

    Serial.println("Received WiFi Credentials:");
    Serial.println(serverWifiCreds[0]);
    Serial.println(serverWifiCreds[1]);

    serverData = true;
    request->redirect("/"); });

    // serve slot labels on web page
    server.on(
        "/slots", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "application/json", slotLabelsAsJson()); });

    // retrive slot labels and  slot factors from webpage
    server.on(
        "/slots", HTTP_POST, [](AsyncWebServerRequest *request)
        {
        String slot_labels[NUM_OF_SLOTS];
        float slot_factors[NUM_OF_SLOTS];
        bool labelsChanged = false;  // Flag to detect label changes
        bool factorsChanged = false; // Flag to detect factor changes

        // Retrieve existing data from NVS
        myNVS::read::slotLabels(slot_labels);
        myNVS::read::slotFactors(slot_factors);

        for (int i = 0; i < NUM_OF_SLOTS; i++)
        {
            String labelParam = "slot-label-" + String(i + 1);
            if (request->hasParam(labelParam, true))
            {
                String newLabel = request->getParam(labelParam, true)->value();
                if (slot_labels[i] != newLabel)  // Check if the label has changed
                {
                    slot_labels[i] = newLabel;
                    labelsChanged = true;  // Set flag to true
                }
                Serial.println(slot_labels[i]);
            }

            String factorParam = "slot-factor-" + String(i + 1);
            if (request->hasParam(factorParam, true))
            {
                float newFactor = atof(request->getParam(factorParam, true)->value().c_str());
                if (slot_factors[i] != newFactor)  // Check if the factor has changed
                {
                    slot_factors[i] = newFactor;
                    factorsChanged = true;  // Set flag to true
                }
                Serial.println(slot_factors[i]);
            }

            // Update in-memory data
            meterData[i].setLabel(slot_labels[i]);
        }

        // Write only if data has changed
        if (labelsChanged)
        {
            myNVS::write::slotLabels(slot_labels);
            Serial.println("Slot labels have been updated!");
            // Update last slot set time regardless of changes
            myNVS::write::lastslotsettime(getUnixTime());
        }

        if (factorsChanged)
        {
            myNVS::write::slotFactors(slot_factors);
            Serial.println("Slot factors have been updated!");
            myNVS::write::lastfactorsettime(getUnixTime());
        }
        // Redirect the user to the home page
        request->redirect("/"); });

    // Wifi, storage and RTC status
    server.on("/getStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    StaticJsonDocument<256> doc;

    String _wifi_check = " ";
    if ((WiFi.isConnected() && ssid.length() > 0) && !(Ping.ping(IPAddress(8, 8, 8, 8), 1)))
    {
       _wifi_check = "Limited!";
    }
    else if((WiFi.isConnected() && ssid.length() > 0) && Ping.ping(IPAddress(8, 8, 8, 8), 1))
    {
       _wifi_check = "\"" + ssid + "\"";
    }
    else
    {
       _wifi_check = "Disconnected!";
    }
    doc["wifi"] = _wifi_check;
    doc["storage"] = flags[sd_f] ? "Storage is working" : "Storage Failed";
    doc["rtc"] = flags[rtc_f] ? "RTC is working" : "RTC Failed";

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

    // restart ESP32 when user clicks restart button
    server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                  request->send(200, "text/plain", "Restarting ESP32...");
                  delay(1000);   // Give time for response to be sent
                  ESP.restart(); // This restarts the ESP32
              });

    // getTime()
    server.on("/get-time", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    String currentTime = getTime();
    request->send(200, "text/plain", currentTime); });

    server.on("/firmware", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", FIRMWARE_VERSION); });

    // not found
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "Not found"); });

    // Password verification endpoint
    server.on("/verify-password", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasParam("password", true)) {
        String inputPassword = request->getParam("password", true)->value();
        
        if (inputPassword == DEFAULT_ADMIN_PASSWORD) {
            request->send(200, "text/plain", "success");
        } else {
            request->send(401, "text/plain", "Invalid password");
        }
    } else {
        request->send(400, "text/plain", "Password required");
    } });

    server.on("/list-files", HTTP_GET, [](AsyncWebServerRequest *request)
              {
    StaticJsonDocument<8192> doc;

    if (!flags[sd_f])
    {
        doc["error"] = "SD card not available";
        String res;
        serializeJson(doc, res);
        request->send(500, "application/json", res);
        return;
    }

    JsonArray rootFiles = doc.createNestedArray("rootFiles");
    JsonArray slotsArr  = doc.createNestedArray("slots");

    unsigned long totalSize = 0;

    File root = SD.open("/");
    if (!root)
    {
        doc["error"] = "Failed to open root directory";
        String res;
        serializeJson(doc, res);
        request->send(500, "application/json", res);
        return;
    }

    File entry;
    while ((entry = root.openNextFile()))
    {
        String entryName = entry.name();

        // ---------------- ROOT FILES (APs.txt etc) ----------------
        if (!entry.isDirectory())
        {
            JsonObject f = rootFiles.createNestedObject();
            f["name"] = entryName;
            f["size"] = entry.size();
            f["type"] = getFileType(entryName);

            totalSize += entry.size();
        }

        // ---------------- SLOT DIRECTORIES ----------------
        else if (entry.isDirectory() && entryName.startsWith("slot"))
        {
            JsonObject slotObj = slotsArr.createNestedObject();
            slotObj["slot"] = entryName;

            JsonArray slotFiles = slotObj.createNestedArray("files");

            File slotDir;
            while ((slotDir = entry.openNextFile()))
            {
                if (!slotDir.isDirectory())
                {
                    JsonObject sf = slotFiles.createNestedObject();
                    sf["name"] = String(slotDir.name());
                    sf["size"] = slotDir.size();
                    sf["type"] = getFileType(slotDir.name());

                    totalSize += slotDir.size();
                }
                slotDir.close();
            }
        }

        entry.close();
    }

    doc["totalSize"] = totalSize;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

    // Download file from SD card
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!request->hasParam("file")) {
            request->send(400, "text/plain", "File parameter missing");
            return;
        }
        
        String fileName = request->getParam("file")->value();
        
        // Security: prevent directory traversal
        if (fileName.indexOf("..") >= 0) {
            request->send(403, "text/plain", "Invalid file path");
            return;
        }
        
        // Ensure filename starts with /
        if (!fileName.startsWith("/")) {
            fileName = "/" + fileName;
        }
        
        if (!SD.exists(fileName)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        
        File file = SD.open(fileName, FILE_READ);
        if (!file) {
            request->send(500, "text/plain", "Failed to open file");
            return;
        }
        
        // Send file as 
        
        request->send(SD, fileName, "application/octet-stream", true);
        file.close(); });

    server.on("/download-range-json", HTTP_GET, handleDownloadRangeJSON);

    server.begin();
}