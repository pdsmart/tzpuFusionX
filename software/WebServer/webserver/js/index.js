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
    showIPConfig();
    enableIfConfig();
});
