var lastStatus = 0;

// Method to display a message in a message field. The existing html is saved and replaced
// with the new html. After a timeout period the original html is restored.
//
$origMessage = null;
$origId = null;
$msgTimerId = null;
function showMessage(timeout, id, message)
{
    // Is this a new message whilst one is active?
    if($origMessage !== null)
    {
        // Cancel timer and restore original message.
        clearTimeout($msgTimerId);
        $('#' + $origId).html($origMessage);
    }

    // Store original message and Id so that on timer expiry it can be replaced..
    $origMessage = $('#' + id).html();
    $origId = id;

    // Change HTML and set timer to restore it.
    $('#' + id).html(message);
    $msgTimerId = setTimeout(function(msgFieldId) 
               {
                    $('#' + msgFieldId).html($origMessage);
                    $origMessage = null;
               }, timeout, id);
}

// Clear the file input array.
function clearFileInput(ctrl)
{
    try {
        ctrl.value = null;
    } catch(ex) { }
    if (ctrl.value) {
        ctrl.parentNode.replaceChild(ctrl.cloneNode(true), ctrl);
    }
}

// Firmware handlers.
document.getElementById('firmwareUpload').onchange = function getFirmwareFileName(e)
{
    var default_path = document.getElementById("firmwareUpload").files[0].name;

    // Put the name of the file into the table cell.
    document.getElementById('firmwareName').innerHTML = "<b>&#61;&gt;</b>" + default_path;
    document.getElementById('firmwareUpgrade').value = document.getElementById('firmwareUpload').files[0].name;

    // Disable select and enable upgrade/cancel.
    document.getElementById('firmwareUploadLabel').style.display = 'none';
    document.getElementById('firmwareUpgrade').disabled = false;
    document.getElementById('firmwareUpgrade').style.display = 'block';
    document.getElementById('firmwareCancel').disabled = false;
    document.getElementById('firmwareCancel').style.display = 'block';
    document.getElementById('firmwareMsg').innerHTML = "Press <b>Upgrade</b> to upload and flash the firmware into the SharpKey or <b>Cancel</b> to cancel and re-select file.";
}
document.getElementById('firmwareCancel').onclick = function cancelFirmwareUpload(e)
{
    var default_path = document.getElementById("firmwareUpload").files[0].name;

    // Reset the selected filename.
    document.getElementById('firmwareName').innerHTML = "";
    document.getElementById('firmwareUpgrade').value = [];
    clearFileInput(document.getElementById('firmwareUpload'));

    // Enable select and disable upgrade/cancel.
    document.getElementById('firmwareUploadLabel').style.display = 'block';
    document.getElementById('firmwareUpgrade').disabled = true;
    document.getElementById('firmwareUpgrade').style.display = 'none';
    document.getElementById('firmwareCancel').disabled = true;
    document.getElementById('firmwareCancel').style.display = 'none';
    document.getElementById('firmwareMsg').innerHTML = "Select a firmware image file with which to upgrade the SharpKey Operating System.";
}

// Firmware upgrade handler.
function firmwareUpdateProgress(e)
{
    if (e.lengthComputable)
    {
        var percentage = Math.round((e.loaded/e.total)*100);
        document.getElementById('firmwareProgressBar').style.width = percentage + '%';
    }
    else 
    {
        document.getElementById('firmwareProgressBar').innerHTML = "Unable to compute progress information since the total size is unknown";
    }
}
document.getElementById('firmwareUpgrade').onclick = function firmwareUpload()
{
    var fileInput = document.getElementById("firmwareUpload").files;

    // Reset the last status. Used to track state change.
    lastStatus = 0;

    // Client side checks, no point initiating a firmware update if the file isnt valid!
    if (fileInput.length == 0)
    {
        alert("No file selected!");
    } else if (fileInput[0].size > 1664*1024)
    {
        alert("File size must be less than 1.6MB!");
    } else 
    {
        document.getElementById("firmwareUpload").disabled = true;
        document.getElementById("firmwareUploadLabel").style.display = 'none';
        document.getElementById("firmwareUpgrade").disabled = true;
        document.getElementById("firmwareUpgrade").style.display = 'none';
        document.getElementById("firmwareCancel").style.display = 'none';
        document.getElementById("firmwareProgress").style.display = 'block';

        var xhttp;
        if(window.XMLHttpRequest)
        {
            xhttp = new XMLHttpRequest();
        } else
         {
            xhttp = new ActiveXObject("Microsoft.XMLHTTP");
        }

        // Install listeners to 
        xhttp.upload.addEventListener("progress", firmwareUpdateProgress, false);
        xhttp.onreadystatechange = function() 
        {
            if (xhttp.readyState == 4) 
            {
                lastStatus = xhttp.status;
                if (xhttp.status == 200) 
                {
                    document.getElementById('firmwareMsg').innerHTML = "<p style=\"color:green;\">Transfer complete, firmware successfully flashed onto the SharpKey. Please press <b>Reboot</b> to activate.</p>";
                    document.getElementById("firmwareProgress").style.display = 'none';
                    document.getElementById('firmwareName').style.display = 'none';
                } else if (xhttp.status == 500) 
                {
                    document.getElementById('firmwareMsg').innerHTML = "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>";
                }
                else if (xhttp.status == 0) 
                {
                    document.getElementById('firmwareMsg').innerHTML = "<p style=\"color:red;\">Error: Server closed the connection abrubtly, flash status unknown! - Press Reboot</p>";
                } else 
                {
                    document.getElementById('firmwareMsg').innerHTML = "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>";
                }
            }
        };
        document.getElementById('firmwareMsg').innerHTML = "<p style=\"color:orange;\">Uploading and flashing the new firmware, please wait...</p>";
        xhttp.open("POST", "/ota/firmware", true);
        xhttp.send(fileInput[0]);
    }
}

// Filepack handlers.
document.getElementById('filepackUpload').onchange = function getFilepackFileName(e)
{
    var default_path = document.getElementById("filepackUpload").files[0].name;

    // Put the name of the file into the table cell.
    document.getElementById('filepackName').innerHTML = "<b>&#61;&gt;</b>" + default_path;
    document.getElementById('filepackUpgrade').value = document.getElementById('filepackUpload').files[0].name;

    // Disable select and enable upgrade/cancel.
    document.getElementById('filepackUploadLabel').style.display = 'none';
    document.getElementById('filepackUpgrade').disabled = false;
    document.getElementById('filepackUpgrade').style.display = 'block';
    document.getElementById('filepackCancel').disabled = false;
    document.getElementById('filepackCancel').style.display = 'block';
    document.getElementById('filepackWarning').style.display = 'block';
    document.getElementById('filepackMsg').innerHTML = "Press <b>Upgrade</b> to upload and flash the filepack onto the SharpKey filesystem or <b>Cancel</b> to cancel and re-select file.";
}
document.getElementById('filepackCancel').onclick = function cancelFilepackUpload(e)
{
    var default_path = document.getElementById("filepackUpload").files[0].name;

    // Reset the selected filename.
    document.getElementById('filepackName').innerHTML = "";
    clearFileInput(document.getElementById('filepackUpload'));

    // Enable select and disable upgrade/cancel.
    document.getElementById('filepackUploadLabel').style.display = 'block';
    document.getElementById('filepackUpgrade').disabled = true;
    document.getElementById('filepackUpgrade').style.display = 'none';
    document.getElementById('filepackCancel').disabled = true;
    document.getElementById('filepackCancel').style.display = 'none';
    document.getElementById('filepackWarning').style.display = 'none';
    document.getElementById('filepackMsg').innerHTML = "Select a filepack image file with which to upgrade the SharpKey filesystem.";
}

// Filepack upgrade handler.
function filepackUpdateProgress(e)
{
    if (e.lengthComputable)
    {
        var percentage = Math.round((e.loaded/e.total)*100);
        document.getElementById('filepackProgressBar').style.width = percentage + '%';
    }
    else 
    {
        document.getElementById('filepackProgressBar').innerHTML = "Unable to compute progress information since the total size is unknown";
    }
}
document.getElementById('filepackUpgrade').onclick = function filepackUpload()
{
    var fileInput = document.getElementById("filepackUpload").files;

    // Reset the last status. Used to track state change.
    lastStatus = 0;

    // Client side checks, no point initiating a filepack update if the file isnt valid!
    if (fileInput.length == 0)
    {
        alert("No file selected!");
    } else if (fileInput[0].size > 640*1024)
    {
        alert("File size must be less than 640K!");
    } else 
    {
        document.getElementById("filepackUpload").disabled = true;
        document.getElementById("filepackUploadLabel").style.display = 'none';
        document.getElementById("filepackUpgrade").disabled = true;
        document.getElementById("filepackUpgrade").style.display = 'none';
        document.getElementById("filepackCancel").style.display = 'none';
        document.getElementById("filepackProgress").style.display = 'block';

        var xhttp;
        if(window.XMLHttpRequest)
        {
            xhttp = new XMLHttpRequest();
        } else
         {
            xhttp = new ActiveXObject("Microsoft.XMLHTTP");
        }

        // Install listeners to 
        xhttp.upload.addEventListener("progress", filepackUpdateProgress, false);
        xhttp.onreadystatechange = function() 
        {
            if (xhttp.readyState == 4) 
            {
                lastStatus = xhttp.status;
                if (xhttp.status == 200) 
                {
                    document.getElementById('filepackMsg').innerHTML = "<p style=\"color:green;\">Transfer complete, filepack successfully flashed onto SharpKey filesystem. Please press <b>Reboot</b> to activate.</p>";
                    document.getElementById("filepackProgress").style.display = 'none';
                    document.getElementById('filepackName').style.display = 'none';
                } else if (xhttp.status == 500) 
                {
                    document.getElementById('filepackMsg').innerHTML = "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>";
                }
                else if (xhttp.status == 0) 
                {
                    document.getElementById('filepackMsg').innerHTML = "<p style=\"color:red;\">Error: Server closed the connection abrubtly, flash status unknown! - Press Reboot</p>";
                } else 
                {
                    document.getElementById('filepackMsg').innerHTML = "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>";
                }
            }
        };
        document.getElementById('filepackMsg').innerHTML = "<p style=\"color:orange;\">Uploading and flashing the new filepack, please wait...</p>";
        xhttp.open("POST", "/ota/filepack", true);
        xhttp.send(fileInput[0]);
    }
}


// Method to enable the correct side-bar menu for the underlying host interface.
function enableIfConfig()
{
    // Disable keymap if no host is connected to the SharpKey. KeyInterface is the base class which exists when
    // no host was detected to invoke a host specific sub-class.
    if(activeInterface === "KeyInterface ")
    {
        document.getElementById("keyMapAvailable").style.display = 'none';
    }
    // Mouse interface active?
    else if(activeInterface === "Mouse ")
    {
        document.getElementById("keyMapAvailable").style.display = 'none';
        document.getElementById("mouseCfgAvailable").style.display = 'compact';
    } else
    {
        document.getElementById("keyMapAvailable").style.display = 'compact';

        // Secondary interface available?
        if(secondaryInterface == "Mouse ")
        {
            document.getElementById("mouseCfgAvailable").style.display = 'compact';
        } else
        {
            document.getElementById("mouseCfgAvailable").style.display = 'none';
        }
    }
}

// On document load, setup the items viewable on the page according to set values.
document.addEventListener("DOMContentLoaded", function setPageDefaults()
{
    enableIfConfig();
});
