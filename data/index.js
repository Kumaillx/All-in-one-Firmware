const numOfSlots = %NUM_OF_SLOTS%;
sensorDataInterval = undefined;

function openTab(tabName) {
    var tabContent = document.getElementsByClassName("tab-content");
    for (var i = 0; i < tabContent.length; i++) {
        tabContent[i].style.display = "none";
    }
    document.getElementById(tabName).style.display = "block";

    clearInterval(sensorDataInterval);

    if (tabName == "tab2") {
        updateSensorData();
        sensorDataInterval = setInterval(updateSensorData, 1000);
    }

    if (tabName == "tab3") {
        updateSlotInfo();
    }
}

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
                <select id="slot-factor-${i}" name="slot-factor-${i}">
            
                    <option value="1.00">100 A</option>
                    <option value="5.00">500 A</option>
                    <option value="10.00">1000 A</option>
                    <option value="20.00">2000 A</option>
                </select>
                <input type="text" id="slot-label-${i}" name="slot-label-${i}" maxlength="100" placeholder="Enter S${i} label">
            </div>
        `;
    }

    slotConfigForm.innerHTML += `
        <div class="form-group">
            <button type="submit">Update</button>               
        </div>
    `;
}

function updateSensorData() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200) {
            var sensorData = JSON.parse(xhr.responseText);

            for (var i = 1; i <= numOfSlots; i++) {

                document.getElementById('slot-label-' + i).textContent = slotInfo['slot-label-' + i];
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

function updateSlotInfo() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4 && xhr.status == 200) {
            console.log(xhr.responseText)
            var slotInfo = JSON.parse(xhr.responseText);

            for (var i = 1; i <= numOfSlots; i++) {
                document.getElementById('slot-label-' + i).value = slotInfo['slot-label-' + i];

                var selectElement = document.getElementById('slot-factor-' + i);
                var desiredValue = slotInfo['slot-factor-' + i];

                for (var j = 0; j < selectElement.options.length; j++) {
                    if (selectElement.options[j].value == desiredValue) {
                        selectElement.selectedIndex = j;
                        break;
                    }
                }
            }
        }
    };
    xhr.open("GET", "/slots", true);
    xhr.send();
}

document.addEventListener("DOMContentLoaded", function () {
    generateTableRows();
    generateSlotConfigRows();
    openTab('tab2');
});