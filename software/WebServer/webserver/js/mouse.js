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

// Method to convert a string/numeric into a hex number and range check it to be within and 8bit unsigned range.
function hexConvert(inVal, invert)
{
    if(isNaN(inVal) == true && isNaN('0x' + inVal) == false)
    {
        inVal = parseInt(inVal, 16);
    }
    else if(isNaN(inVal) == false && typeof inVal == 'string' && inVal.length > 0)
    {
        if(inVal.toUpperCase().indexOf("0X") == 0)
        {
            inVal = parseInt(inVal, 16);
        } else
        {
            inVal = parseInt(inVal, 10);
        }
    }
    else if(isNaN(inVal) == true || inVal.length == 0)
    {
        inVal = 0;
    }
    if(inVal < 0)
    {
        inVal = 0;
    }
    else if(inVal > 255)
    {
        inVal = 255;
    }
    if(invert == true)
    {
        inVal = (~inVal) & 0xFF;
    }
    return('0x' + (inVal).toString(16).toUpperCase());
}

$(document).ready(function() {

    // Setup the menu options according to underlying interface.
    enableIfConfig();

    // AJAX code to post in a controlled manner so that we can receive back an error/success message to the commit.
    $("#mouseHostCfgSave").submit( function(e)
    {
        var form = $(this);
        var actionUrl = form.attr('action');

        // Prevent default submit action, we want to manually submit and be able to receive a response for errors/success.
        e.preventDefault();
        
        $.ajax(
        {
            type: "POST",
            url: actionUrl,
            data: form.serialize(), // serializes the form's elements.
            success: function(data)
                     {
                         // Show the message then revert back to original text.
                         showMessage(10000, "mouseHostCfgMsg", data);
                     }
         });
    });
    $("#mousePS2CfgSave").submit( function(e)
    {
        var form = $(this);
        var actionUrl = form.attr('action');

        // Prevent default submit action, we want to manually submit and be able to receive a response for errors/success.
        e.preventDefault();
        
        $.ajax(
        {
            type: "POST",
            url: actionUrl,
            data: form.serialize(), // serializes the form's elements.
            success: function(data)
                     {
                         // Show the message then revert back to original text.
                         showMessage(10000, "mousePS2CfgMsg", data);
                     }
         });
    });
});
