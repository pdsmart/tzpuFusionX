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


function showWiFiAPInput()
{
    document.getElementById('inputWiFiClient').style.display = 'none';
    document.getElementById('inputWiFiAP').style.display = 'block';
    document.getElementById("errorMsgAP").innerHTML = "";
}

function showWiFiClientInput()
{
    document.getElementById('inputWiFiClient').style.display = 'block';
    document.getElementById('inputWiFiAP').style.display = 'none';
    document.getElementById("errorMsgClient").innerHTML = "";
}

function hideFixedIPInput()
{
    document.getElementById('rowClientIP').style.display = 'none';
    document.getElementById('rowClientNETMASK').style.display = 'none';
    document.getElementById('rowClientGATEWAY').style.display = 'none';
    document.getElementById('dhcpInput').style.display = 'block';
    document.getElementById("errorMsgClient").innerHTML = "";
}

function showFixedIPInput()
{
    document.getElementById('rowClientIP').style.display = 'table-row';
    document.getElementById('rowClientNETMASK').style.display = 'table-row';
    document.getElementById('rowClientGATEWAY').style.display = 'table-row';
    document.getElementById('dhcpInput').style.display = 'none';
    document.getElementById("errorMsgClient").innerHTML = "";
}

function showIPConfig() 
{
    if(document.getElementById("wifiCfg0checked"))
    {
        document.getElementById("wifiCfg0checked").style.display = 'compact';
        document.getElementById("wifiCfg").style.display = 'none';

        if(document.getElementById("wifiCfg3checked"))
        {
          document.getElementById("wifiCfg3checked").style.display = 'compact';
          document.getElementById("wifiCfg3").style.display = 'none';
        } else
        {
          document.getElementById("wifiCfg3checked").style.display = 'none';
          document.getElementById("wifiCfg3").style.display = 'compact';
        }

        if(document.getElementById("wifiCfg1checked"))
        {
          document.getElementById("wifiCfg1checked").style.display = 'compact';
        } else
        {
          document.getElementById("wifiCfg1").style.display = 'none';
        }
    } else
    {
      document.getElementById("wifiCfg0").style.display = 'none';
      document.getElementById("wifiCfgchecked").style.display = 'compact';
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
    document.getElementById('wifiModeAccessPoint').onclick = showWiFiAPInput;
    document.getElementById('wifiModeClient').onclick = showWiFiClientInput;
    document.getElementById('dhcpModeEnabled').onclick = hideFixedIPInput;
    document.getElementById('dhcpModeDisabled').onclick = showFixedIPInput;

    // Setup AP/Client display.
    if(document.getElementById('wifiModeClient').checked)
    {
        showWiFiClientInput();
    } else
    {
        showWiFiAPInput();
    }
    if(document.getElementById('dhcpModeEnabled').checked)
    {
        hideFixedIPInput();
    } else
    {
        showFixedIPInput();
    }

    // AJAX code to post in a controlled manner so that we can receive back an error/success message to the commit.
    $("#wifiman").submit( function(e)
    {
        var form = $(this);
        var actionUrl = form.attr('action');
    
        // Prevent default submit action, we want to manually submit and be able to receive a response for errors/success.
        e.preventDefault();
        
        // Clear message window before making a POST, allows for a new message if one is provided or blank if it isnt.
        document.getElementById("errorMsgClient").innerHTML = "";
        document.getElementById("errorMsgAP").innerHTML = "";
    
        $.ajax(
        {
            type: "POST",
            url: actionUrl,
            data: form.serialize(), // serializes the form's elements.
            success: function(data)
                     {
                         // JQuery not rendering HTML correcly so revert to DOM.
                         document.getElementById("errorMsgClient").innerHTML = data;
                         document.getElementById("errorMsgAP").innerHTML = data;
                         //form.find('#errorMsg').html(data);
                     }
         });
    });

    showIPConfig();
    enableIfConfig();
});
