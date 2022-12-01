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

// Ajax download function, from user:leo on stackexchange, customised for this application.
//
function downloadFile()
{
    var req = new XMLHttpRequest();
    var downloadUrl = "/keymap";
    req.open("GET", downloadUrl, true);
    req.responseType = "blob";
    
    req.onload = function (event)
    {
        var blob = req.response;
        var fileName = null;
        var contentType = req.getResponseHeader("content-type");
    
        // IE/EDGE seems not returning some response header
        if (req.getResponseHeader("content-disposition")) 
        {
            var contentDisposition = req.getResponseHeader("content-disposition");
            fileName = contentDisposition.substring(contentDisposition.indexOf("=")+1);
        } else 
        {
            fileName = "unnamed." + contentType.substring(contentType.indexOf("/")+1);
        }
      
        if (window.navigator.msSaveOrOpenBlob)
        {
            // Internet Explorer
            window.navigator.msSaveOrOpenBlob(new Blob([blob], {type: contentType}), fileName);
        } else 
        {
            var el = document.getElementById("keymapDownloadLink");
            el.href = window.URL.createObjectURL(blob);
            el.download = fileName;
            el.click();
        }
    };
    showMessage(1000, 'keymapMsg', "<p style=\"color:orange;\">Downloading keymap file, please wait...</p>");
    req.send();
}

// Listener for the Download button click. Once clicked commence download via ajax function.
//
document.getElementById('keymapDownload').onclick = function keymapFileDownload(e)
{
    downloadFile();
}


// Listener for an event change on the INPUT file tag 'keymapUpload'. Once pressed and a filename available, this method
// commences transmission via a POST method to the SharpKey server.
document.getElementById('keymapUpload').onchange = function keymapFileUpload(e)
{
    var fileInput = document.getElementById("keymapUpload").files;

    // Reset the last status. Used to track state change.
    lastStatus = 0;

    // Client side checks, no point initiating an upload if the file isnt valid!
    if (fileInput.length == 0)
    {
        alert("No file selected!");

    // Sane size check, a keymap entry is 6-12 bytes long, so a 1000 rows should be the max size.
    } else if (fileInput[0].size > 12*1024)
    {
        alert("File size must be less than 12KB!");
    } else 
    {
        var xhttp;
        if(window.XMLHttpRequest)
        {
            xhttp = new XMLHttpRequest();
        } else
         {
            xhttp = new ActiveXObject("Microsoft.XMLHTTP");
        }

        // Install listeners to handle state change.
        xhttp.onreadystatechange = function() 
        {
            if (xhttp.readyState == 4) 
            {
                lastStatus = xhttp.status;
                if (xhttp.status == 200) 
                {
                    showMessage(5000, 'keymapMsg', "<p style=\"color:green;\">Upload complete. Please press <b>Reboot</b> to activate new key map.</p>");
                } else if (xhttp.status == 500) 
                {
                    showMessage(5000, 'keymapMsg', "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>");
                }
                else if (xhttp.status == 0) 
                {
                    showMessage(5000, 'keymapMsg', "<p style=\"color:red;\">Error: Server closed the connection abrubtly, status unknown! - Please retry or press Reboot</p>");
                } else 
                {
                    showMessage(5000, 'keymapMsg', "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>");
                }
            }
        };
        showMessage(10000, 'keymapMsg', "<p style=\"color:orange;\">Uploading keymap file, please wait...</p>");
        xhttp.open("POST", "/keymap", true);
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

// Method to validate a hex input field. Basically call hexConvert and it will return a
// valid number, 0 if invalid or 255 if out of range.
function validateHexInput(event) 
{
    event.target.value=hexConvert(event.target.value, false);
    return true;
};

// Use variables to remember last popover and value as the popover mechanism, probably due to race states, isnt uniform.
// Also use the shown/hidden events and toggle as hide doesnt trigger reliably.
var $currentPopover = null;
var $currentPopoverVal = -1;
var $currentClass = null;
var $currentSelectCount = 0;
var $currentTimerId = null;
var $currentDataSetModified = false;

function updateButtons()
{
    if($currentSelectCount == 2)
    {
        $('#keymapSwap').removeAttr('disabled');
    } else
    {
        $('#keymapSwap').attr('disabled', 'disabled');
    }
    if($currentSelectCount > 0)
    {
        $('#keymapDelete').removeAttr('disabled');
    } else
    {
        $('#keymapDelete').attr('disabled', 'disabled');
    }
    if($currentDataSetModified == true)
    {
        $('#keymapSave').removeAttr('disabled');
        $('#keymapReload').removeAttr('disabled');
    } else
    {
        $('#keymapSave').attr('disabled', 'disabled');
        $('#keymapReload').attr('disabled', 'disabled');
    }
}

function addPopover(field, tableRow)
{

    // Popover auto-hide timer alarm.
    const popoverAlarm = {

        setup: function(timerId, timeout) {
            if (timerId != null) {
                timerId = this.cancel(timerId);
            }

            timerId = setTimeout(function() {

               if($currentPopover) {
                 $currentPopover.popover('toggle');
               }
            }, timeout);

            return(timerId);
        },

        cancel: function(timerId) {
            clearTimeout(timerId);
            timerId = null;
            return(timerId);
        },
    };

    // Setup popover menus on each custom field to aid with conversion of a feature into a hex number.
    $(field).popover({
                        html: true,
                        title: function () {
   
                            return $('#popover-' + field.className).find('.head').html();
                        },
                        content: function () {
                            return $('#popover-' + field.className).find('.content').html();
                        }
           })
           .on("show.bs.popover", function(e)
           {
               var className = this.getAttribute('class');
               var $target = $(e.target);

               // Detect a class change, user has gone from one column to another, need to close out current
               // popover ready for initialisation of new.
               if($currentClass != null && $currentClass != className)
               {
                   if($currentPopover)
                   {
                       if($currentTimerId != 0)
                       {
                           $currentTimerId = popoverAlarm.cancel($currentTimerId);
                       }

                       if ($currentPopover && ($currentPopover.get(0) != $target.get(0))) {
                           $currentPopover.popover('toggle');
                       }
                       $currentPopover = null;
                   }
               }
               $currentClass = className;

               if($currentPopover)
               {
                   $currentPopoverVal = 0;
                   $('#popover-' + field.className).find('input').each( function( index ) {

                       if($(this).attr("checked") === "checked")
                       {
                           $currentPopoverVal += parseInt(this.getAttribute('data-value'), 10);
                       }
                   });
               } else
               {
                   $currentPopoverVal = -1;
               }

               // Go through all the checkboxes and setup the initial value.
               $('#popover-' + field.className).find('input').each( function( index ) {
                   if((e.target.value & this.getAttribute('data-value')) == this.getAttribute('data-value'))
                   {
                       $(this).attr('checked', true);
                   } else
                   {
                       $(this).attr('checked', false);
                   }
               });
           })
           .on("shown.bs.popover", function(e)
           {
               var $target = $(e.target);

               if ($currentPopover && ($currentPopover.get(0) != $target.get(0))) {
                 $currentPopover.popover('toggle');
               }
               $currentPopover = $target;
               
               // Setup an alarm to remove a visible popover if not clicked closed.    
               $currentTimerId = popoverAlarm.setup($currentTimerId, 5000);
           })
           .on("hidden.bs.popover", function(e)
           {
               var $target = $(e.target);

               if ($currentPopover && ($currentPopover.get(0) == $target.get(0))) {
                 if($currentPopoverVal == -1)
                 {
                     $currentPopoverVal = 0;
                     $('#popover-' + field.className).find('input').each( function( index ) {

                         if($(this).attr("checked") === "checked")
                         {
                             $currentPopoverVal += parseInt(this.getAttribute('data-value'), 10);
                         }
                     });
                 }

                 if(e.target.value != hexConvert($currentPopoverVal, false))
                 {
                     e.target.value = hexConvert($currentPopoverVal, false);
                     $currentDataSetModified = true;
                     updateButtons();
                 }
                 $currentPopover = null;
                 // Cancel the alarm on the hidden popover.
                 $currentTimerId = popoverAlarm.cancel($currentTimerId);
               }
               $currentPopoverVal = -1;
           })

           .on("hide.bs.popover", function(e)
           {
             //  $currentTimerId = popoverAlarm.cancel($currentTimerId);
           })
           .on("click", function(e)
           {
               console.log('click: ' + e.target.value);
           })
           .on("change", function(e)
           {
               if(e.target.className === 'checkbox')
               {
                   if($(e.target).attr('checked') == 'checked')
                   {
                       $currentSelectCount += 1;
                       //$(e.target).attr("checked", true);
                   }
                   else if($(e.target).attr('checked') != 'checked')
                   {
                       $currentSelectCount -= 1;
                       //$(e.target).attr("checked", false);
                   }
               } else
               {
                   if(e.target.value != hexConvert($currentPopoverVal, false))
                   {
                       e.target.value = hexConvert(e.target.value, false);
                       $currentDataSetModified = true;
                   }
               }
               updateButtons();
           })

           .blur(function () {
           });
    // End popover definition
    
    // Only attach popover handlers to classes in the first row, subsequent rows will use the same popover.
    //
    if(tableRow == 1)
    {
        var search = '#popover-' + field.className;
        $(search + ' input').each(function () {

            $('body').on('click', '#' + this.id, function(e) {

                $currentTimerId = popoverAlarm.setup($currentTimerId, 5000);

                $(search + ' input').each(function() {
                    if(this.id === e.target.id)
                    {
                        $(this).attr("checked", e.target.checked ? true : false);
                    }
                });
            });
        });
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

// Method to swap 2 tables rows. This is necessary as the keymap table has top down priority in mapping.
function swapKeyMapData()
{
    // Locals.
    var firstRow = null;
    var secondRow = null;

    // Get row numbers of the rows to be swapped.
    var rowcnt = 1;
    $('.inputtable tr .checkbox').each(function() {
        if(firstRow == null && this.checked == true)
        {
            firstRow = rowcnt;
        } else
        if(firstRow != null && secondRow == null && this.checked == true)
        {
            secondRow = rowcnt;
        }
        rowcnt++;
    });

    // If rows were matched (should be due to count) then perform swap.
    if(firstRow != null && secondRow != null)
    {
        $('.inputtable > tbody > tr:nth-child(' + firstRow + ')').replaceWith($('.inputtable > tbody > tr:nth-child(' + secondRow + ')').after($('.inputtable > tbody > tr:nth-child(' + firstRow + ')').clone(true)));
    }
   
    // Update the modified state and update button state.
    $currentDataSetModified = true;
    updateButtons();

    return true;
}

// Method to delete 1 or multiple rows. An individual row can be deleted by the +/- buttons but multiple requires selection and then running through the table, deleting the selected rows.
function deleteKeyMapData()
{
    // Locals.
    var toDeleteRows = [];

    // Get row numbers of the rows to be swapped.
    var rowcnt = 1;
    $('.inputtable tr .checkbox').each(function() {
        if(this.checked == true)
        {
            toDeleteRows.push(rowcnt);
        } 
        rowcnt++;
    });

    // Iterate through the array of rows to delete and actually perform the deletion.
    // We work in reverse order, ie. deleting the last row first due to the index changing if we delete first to last.
    for(var idx = toDeleteRows.length; idx > 0; idx--)
    {
        $('.inputtable > tbody > tr:nth-child(' + toDeleteRows[idx-1] + ')').remove();
    }

    // Reset the select counter and update button state.
    $currentSelectCount = 0;
    $currentDataSetModified = true;
    updateButtons();

    return true;
}

// Method to save the current keymap table data to the interface.
function saveKeyMapData()
{
    var data = keymapTable.getData();

    // Reset the last status. Used to track state change.
    lastStatus = 0;

    // Client side checks, no point initiating an upload if the file isnt valid!
    if(data.length == 0)
    {
        alert("No data to save!!");

    } else 
    {
        var xhttp;
        if(window.XMLHttpRequest)
        {
            xhttp = new XMLHttpRequest();
        } else
         {
            xhttp = new ActiveXObject("Microsoft.XMLHTTP");
        }

        // Install listeners to handle state change.
        xhttp.onreadystatechange = function() 
        {
            if (xhttp.readyState == 4) 
            {
                lastStatus = xhttp.status;
                if (xhttp.status == 200) 
                {
                    showMessage(5000, 'keymapEditorMsg', "<p style=\"color:green;\">Upload complete. Please press <b>Reboot</b> to activate new key map.</p>");
                   
                    // Reset the data modified flag and update button state.
                    $currentDataSetModified = false;
                    updateButtons();
                } else if (xhttp.status == 500) 
                {
                    showMessage(5000, 'keymapEditorMsg', "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>");
                }
                else if (xhttp.status == 0) 
                {
                    showMessage(5000, 'keymapEditorMsg', "<p style=\"color:red;\">Error: Server closed the connection abrubtly, status unknown! - Please retry Save</p>");
                } else 
                {
                    showMessage(5000, 'keymapEditorMsg', "<p style=\"color:red;\">Error: " + xhttp.responseText + " - Press Reboot</p>");
                }
            }
        };
        showMessage(5000, 'keymapEditorMsg', "<p style=\"color:orange;\">Uploading keymap data, please wait...</p>");
        xhttp.open("POST", "/keymap/table", true);
        xhttp.send(JSON.stringify(data));
    }
   
    return true;
}

// Method to reload the initial table data set.
function reloadKeyMapData()
{
    // Request reload of the initial data.
    keymapTable.reset();

    // Re-install the listeners as the old dataset was deleted.
    setupTableListeners();

    // Complete message.
    showMessage(5000, 'keymapEditorMsg', "<p style=\"color:green;\">Reload complete, data reset to initial values.</p>");
    
    // Reset the select counter and update button state.
    $currentSelectCount = 0;
    $currentDataSetModified = false;
    updateButtons();
    return true;
}

// Callback function triggered after a row has been added to the edittable. Use the event to add popover listeners to the new fields.
//
function tableRowAdded(row, rowHandle, tableHandle)
{
    $(rowHandle).children().children().each(function() {
        if(this.className !== "")
        {
            addPopover(this, 0);
        }
    });
}

// Method to install listeners on the table data for popover aids.
//
function setupTableListeners()
{
    // Setup a popover on each input field where a modal exists. 
    $('.inputtable >tbody > tr').each(function (tridx) {

        // We seperate out the Row from the input as we want to watch every input but only setup 1 popover per input type.
        $(this).find('input').each(function (inpidx) {
            if(this.className !== "")
            {
                addPopover(this, tridx);
            }
        });
    });
    //
    // Setup a callback on new rows added to table.
    keymapTable.addRowCallBack(tableRowAdded);
}

$(document).ready(function() {

    // Load up the table headers, types and data. This is done with JQuery fetch as it is easier. The SharpKey is synchronous
    // but fetch order can vary so we nest the requests to ensure all headers and types are in place before the data.
    fetch('/data/keymap/table/headers')
      .then((response) => {
          return(response.json());
      })
      .then((headers) => {
          keymapTable.setHeaders(headers);

          fetch('/data/keymap/table/types')
            .then((response) => {
                return(response.json());
            })
            .then((types) => {
                keymapTable.setTypes(types);

                fetch('/data/keymap/table/data')
                  .then((response) => {
                      return(response.json());
                  })
                  .then((data) => {
                      keymapTable.loadData(data);
                      setupTableListeners();
                  });
            });
      });

    // Disable buttons, enabled when user action requires them.
    $('#keymapSwap').attr('disabled', 'disabled');
    $('#keymapDelete').attr('disabled', 'disabled');
    $('#keymapSave').attr('disabled', 'disabled');
    $('#keymapReload').attr('disabled', 'disabled');

    // Add listeners to the buttons.
    $('#keymapSwap').on('click', function() { swapKeyMapData(); });
    $('#keymapDelete').on('click', function() { deleteKeyMapData(); });
    $('#keymapSave').on('click', function() { saveKeyMapData(); });
    $('#keymapReload').on('click', function() { reloadKeyMapData(); });
   
    // Setup the menu options according to underlying interface.
    enableIfConfig();
});
