// document.getElementById("hide").focus();
var keys={
  48:0,
  49:1,
  50:2,
  51:3,
  52:4,
  53:5,
  54:6,
  55:7,
  56:8,
  57:9
}
document.onkeydown = function (e) {
  if (e.keyCode == 462 || e.keyCode == 8 || e.keyCode == 27) {
    ime_cmd("hide");
    console.log(e.keyCode);
  }
  if(keys.hasOwnProperty(e.keyCode)){
    ime_insert(keys[e.keyCode]);
  }
}
function hide(){
  ime_cmd("hide");
}
var htmlCode = {
    "&amp;": "&",
    "&lt;": "<",
    "&gt;": ">",
    "Space": " ",
  }
  var lang = "eng";

  function ime_cmd(v) {
    if (window.iBrowser !== undefined) {
      window.iBrowser.sendCommand(v, "");
    } else {
      console.log("IME:[CMD]:" + v);

    }
  }

  function ime_insert(v) {
    if (window.iBrowser !== undefined) {
      window.iBrowser.sendCommand("insert", v);
    } else {
      console.log("IME:[INS]:" + v);
    }
  }

  function test() {
    var input = document.getElementById("input");
    var e = window.event || test.caller.arguments[0];
    var el = e.target || e.srcElement;
    if (el.tagName.toLowerCase() == "button") {
      if (el.className === "" || el.className === undefined || el.className === "num") {
        var str = el.innerHTML;
        str = htmlCode[str] || str;
        ime_insert(str);
      } else if (el.className == "ctrlkeys") {
        ime_cmd(el.id);
      } else if (el.id == "shift") {
        var els = document.getElementsByTagName("button");
        for (var i = 0, l = els.length; i < l; i++) {
          var str = els[i].innerHTML;
          if (/^[a-z]$/.test(str))
            els[i].innerHTML = str.toUpperCase();
          if (/^[A-Z]$/.test(str))
            els[i].innerHTML = str.toLowerCase();
        }
      } else if (el.id == "change") {
        if (lang == "eng") return;
        document.getElementsByTagName("table")[0].style.display = "none";
        Keyboard.show();
      }
    }

  }

  function ctrKeyboard() {
    capsInit();
  }

  function capsInit() {
    var els = document.getElementsByTagName("button");
    for (var i = 0, j = 0, l = els.length; i < l; i++) {
      var str = els[i].innerHTML;
      if (/^[A-Z]$/.test(str))
        els[i].innerHTML = str.toLowerCase();
    }
  }

  function init() {
    //添加语言切换个数
    screenZoom();
    var index=1;
    lang = window.navigator.language?window.navigator.language:"eng";
    console.log("window.navigator.language:"+lang);
    var actindex=0;
    for (var key in Keyboard.layouts) {
      console.log(Keyboard.layouts[key].name);
      var lis = document.createElement('li');
      lis.innerHTML = Keyboard.layouts[key].name;
      lis.setAttribute('lang',Keyboard.layouts[key].lang);
      lis.setAttribute('data-index',index);
      document.getElementById('language').appendChild(lis)
      if(lang==key) actindex=index;
      index++;
    }
    var el=document.querySelector(".language .active");
    var next=document.querySelectorAll(".language li")[actindex];
    if(next){
      showlanguage(el,next);
    }
    else{
      KeyboardInit();
    }
  }
  
  function screenZoom () {
  var $box = document.getElementsByClassName('main');
  var w = 1280;
  var h = 720;
  var W = window.innerWidth ;
  for(var i=0;i<$box.length;i++){
    $box[i].style.zoom= W / w;
  }
}
  function KeyboardInit(){
    capsInit();
    // console.log("lang:"+window.navigator.language);
    // lang = window.navigator.language?window.navigator.language:"eng";//语言
     //lang="rus";
     console.log("lang:"+lang);
    if (Keyboard.layouts[lang]) {
      Keyboard.init(lang, function (type, msg) {
        if (type == "insert") {
          ime_insert(msg);
        } else if (type == "cmd") {
          ime_cmd(msg);
        } else {
          document.getElementsByTagName("table")[0].style.display = "";
        }
      });
      document.getElementsByTagName("table")[0].style.display = "none";
      //Keyboard.hide();
    } else {
      lang = "eng";
      var space = document.getElementById("thespace");
      // space.nextElementSibling.style.display="none";
      // space.colSpan='3';
      document.getElementsByTagName("table")[0].style.display = "";
      Keyboard.hide()
    }
  }
  function left(){
      var el=document.querySelector(".language .active");
      var next=el.nextSibling;
      if(next) showlanguage(el,next);
  }
  function right(){
        var el=document.querySelector(".language .active");
        var next=el.previousSibling;
        if(next) showlanguage(el,next);
        
  }
  function showlanguage(el,next){
        var index =next.dataset.index;
        console.log(index);
        el.className="";
        next.className="active";
        var languageEl=document.getElementById("language");
        languageEl.style.marginLeft=(-el.clientWidth * index)+"px";
        lang=next.lang;
        KeyboardInit();
  }