/**
 * Add gobals here
 */
var seconds = null;
var otaTimerVar = null;
var wifiConnectInterval = null;

/**
 * Initialize functions here.
 */
$(document).ready(function () {
  $("#connect_wifi").on("click", function () {
    checkCredentials();
  });
});

/**
 * Gets file name and size for display on the web page.
 */
function getFileInfo() {
  var x = document.getElementById("selected_file");
  var file = x.files[0];

  document.getElementById("file_info").innerHTML =
    "<h4>File: " + file.name + "<br>" + "Size: " + file.size + " bytes</h4>";
}

/**
 * Handles the firmware update.
 */
function updateFirmware() {
  // Form Data
  var formData = new FormData();
  var fileSelect = document.getElementById("selected_file");

  if (fileSelect.files && fileSelect.files.length == 1) {
    var file = fileSelect.files[0];
    formData.set("file", file, file.name);
    document.getElementById("ota_update_status").innerHTML =
      "Uploading " + file.name + ", Firmware Update in Progress...";

    // Http Request
    var request = new XMLHttpRequest();

    request.upload.addEventListener("progress", updateProgress);
    request.open("POST", "/OTAupdate");
    request.responseType = "blob";
    request.send(formData);
  } else {
    window.alert("Select A File First");
  }
}

/**
 * Progress on transfers from the server to the client (downloads).
 */
function updateProgress(oEvent) {
  if (oEvent.lengthComputable) {
    getUpdateStatus();
  } else {
    window.alert("total size is unknown");
  }
}

/**
 * Displays the reboot countdown.
 */
function otaRebootTimer() {
  document.getElementById("ota_update_status").innerHTML =
    "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " +
    seconds;

  if (--seconds == 0) {
    clearTimeout(otaTimerVar);
    window.location.reload();
  } else {
    otaTimerVar = setTimeout(otaRebootTimer, 1000);
  }
}

function updateADCValue() {
  fetch("/adc_value")
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok " + response.statusText);
      }
      return response.text();
    })
    .then((data) => {
      console.log("ADC Value:", data); // Logging ADC value to the console
      document.getElementById("adcValue").innerText = data;

      let value = parseInt(data);

      // Define your range
      let lowerLimit = 0;
      let upperLimit = 30;

      // Get the element you want to change the color of
      let element = document.getElementById("dot");

      // Check if value is within range
      if (value >= lowerLimit && value <= upperLimit) {
        // Change color to green if within range
        element.style.backgroundColor = "green";
      } else if (value > upperLimit) {
        // Change color to red if out of range
        element.style.backgroundColor = "red";
      } else {
        element.style.backgroundColor = "blue";
      }
    })
    .catch((error) => {
      console.error("There was a problem with the fetch operation:", error);
    });
}
// Update every 2 seconds
setInterval(updateADCValue, 500);

/**
 * Clears the connection status interval.
 */
function stopWifiConnectStatusInterval() {
  if (wifiConnectInterval != null) {
    clearInterval(wifiConnectInterval);
    wifiConnectInterval = null;
  }
}

/**
 * Gets the WiFi connection status.
 */
function getWifiConnectStatus() {
  var xhr = new XMLHttpRequest();
  var requestURL = "/wifiConnectStatus";
  xhr.open("POST", requestURL, false);
  xhr.send("wifi_connect_status");

  if (xhr.readyState == 4 && xhr.status == 200) {
    var response = JSON.parse(xhr.responseText);

    document.getElementById("wifi_connect_status").innerHTML = "Conectando...";

    if (response.wifi_connect_status == 2) {
      document.getElementById("wifi_connect_status").innerHTML =
        "<h4 class='rd'>Failed to Connect. Please check your AP credentials and compatibility</h4>";
      stopWifiConnectStatusInterval();
    } else if (response.wifi_connect_status == 3) {
      document.getElementById("wifi_connect_status").innerHTML =
        "Conexión Exitosa";
      document.getElementById("WiFiForm").style.display = "none";
      document.getElementById("dot_wifi").style.display = "block";

      fetch("/ntp_value")
        .then((response) => {
          if (!response.ok) {
            throw new Error(
              "Network response was not ok " + response.statusText
            );
          }
          return response.text();
        })
        .then((data) => {
          console.log("Ntp value:", data);
          document.getElementById("ntp_time").innerText = data;
          document.getElementById("ntp_time").style.display = "block";
        })
        .catch((error) => {
          console.error("There was a problem with the fetch operation:", error);
        });

      stopWifiConnectStatusInterval();
    }
  }
}

/**
 * Starts the interval for checking the connection status.
 */
function startWifiConnectStatusInterval() {
  wifiConnectInterval = setInterval(getWifiConnectStatus, 2800);
}

/**
 * Connect WiFi function called using the SSID and password entered into the text fields.
 */
function connectWifi() {
  // Get the SSID and password
  /*selectedSSID = $("#connect_ssid").val();
	pwd = $("#connect_pass").val();
	
	$.ajax({
		url: '/wifiConnect.json',
		dataType: 'json',
		method: 'POST',
		cache: false,
		headers: {'my-connect-ssid': selectedSSID, 'my-connect-pwd': pwd},
		data: {'timestamp': Date.now()}
	});
	*/
  selectedSSID = $("#connect_ssid").val();
  pwd = $("#connect_pass").val();

  // Create an object to hold the data to be sent in the request body
  var requestData = {
    selectedSSID: selectedSSID,
    pwd: pwd,
    timestamp: Date.now(),
  };

  // Serialize the data object to JSON
  var requestDataJSON = JSON.stringify(requestData);

  $.ajax({
    url: "/wifiConnect.json",
    dataType: "json",
    method: "POST",
    cache: false,
    data: requestDataJSON, // Send the JSON data in the request body
    contentType: "application/json", // Set the content type to JSON
    success: function (response) {
      // Handle the success response from the server
      console.log(response);
    },
    error: function (xhr, status, error) {
      // Handle errors
      console.error(xhr.responseText);
    },
  });

  startWifiConnectStatusInterval();
}

/**
 * Checks credentials on connect_wifi button click.
 */
function checkCredentials() {
  errorList = "";
  credsOk = true;

  selectedSSID = $("#connect_ssid").val();
  pwd = $("#connect_pass").val();

  if (selectedSSID == "") {
    errorList += "<h4 class='rd'>SSID cannot be empty!</h4>";
    credsOk = false;
  }
  if (pwd == "") {
    errorList += "<h4 class='rd'>Password cannot be empty!</h4>";
    credsOk = false;
  }

  if (credsOk == false) {
    $("#wifi_connect_credentials_errors").html(errorList);
  } else {
    $("#wifi_connect_credentials_errors").html("");
    connectWifi();
  }
}

/**
 * Shows the WiFi password if the box is checked.
 */
function showPassword() {
  var x = document.getElementById("connect_pass");
  if (x.type === "password") {
    x.type = "text";
  } else {
    x.type = "password";
  }
}

function updateRange() {
  high_temp_lvalue = $("#red_led_range_l").val();
  high_temp_uvalue = $("#red_led_range_u").val();

  medium_temp_lvalue = $("#green_led_range_l").val();
  medium_temp_uvalue = $("#green_led_range_u").val();

  low_temp_lvalue = $("#blue_led_range_l").val();
  low_temp_uvalue = $("#blue_led_range_u").val();

  r_value_first_led = $("#red_led_R").val();
  g_value_first_led = $("#red_led_G").val();
  b_value_first_led = $("#red_led_B").val();

  r_value_second_led = $("#green_led_R").val();
  g_value_second_led = $("#green_led_G").val();
  b_value_second_led = $("#green_led_B").val();

  r_value_third_led = $("#blue_led_R").val();
  g_value_third_led = $("#blue_led_G").val();
  b_value_third_led = $("#blue_led_B").val();

  // Create an object to hold the data to be sent in the request body
  var requestData = {
    high_temp_lvalue: high_temp_lvalue,
    high_temp_uvalue: high_temp_uvalue,
    medium_temp_lvalue: medium_temp_lvalue,
    medium_temp_uvalue: medium_temp_uvalue,
    low_temp_lvalue: low_temp_lvalue,
    low_temp_uvalue: low_temp_uvalue,
    r_value_first_led: r_value_first_led,
    g_value_first_led: g_value_first_led,
    b_value_first_led: b_value_first_led,
    r_value_second_led: r_value_second_led,
    g_value_second_led: g_value_second_led,
    b_value_second_led: b_value_second_led,
    r_value_third_led: r_value_third_led,
    g_value_third_led: g_value_third_led,
    b_value_third_led: b_value_third_led,
  };

  console.log(high_temp_lvalue, high_temp_uvalue);
  // Serialize the data object to JSON
  var requestDataJSON = JSON.stringify(requestData);

  $.ajax({
    url: "/tempRange.json",
    dataType: "json",
    method: "POST",
    cache: false,
    data: requestDataJSON, // Send the JSON data in the request body
    contentType: "application/json", // Set the content type to JSON
    success: function (response) {
      // Handle the success response from the server
      console.log(response);
    },
    error: function (xhr, status, error) {
      // Handle errors
      console.error(xhr.responseText);
    },
  });

  // startWifiConnectStatusInterval();
}
