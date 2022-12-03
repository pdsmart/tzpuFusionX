require.config({
    baseUrl: "./js",
    paths: {
        "jquery": "jquery.min",
    },
    shim: {
        'config': {
            deps: [''],
            exports: 'config'
        },
    },
    waitSeconds: 0
})
var KEY={
    38: 'up',
    40: 'down',
    37: 'left',
    39: 'right',
    13: 'ok',
    462: 'return',
    8: 'return',
    27: 'return'
};
var dom_list={
    data:{}
};
var page;
require(['jquery','details','config'], function ($,details) {
    dom_list.bind=function(name,data){
            this.data[name]=data;
        }
    $.fn.Focus = function () {
        if ($(this).length !== 1) return false;
        $('.Focus').removeClass('Focus');
        $(this).addClass('Focus');
        return $(this);
    };
    page=$("#recomend");
    init();
    $("#menu li[data-url=recomend]").Focus();
    $("#main").focus();
    function openContent($Focus) {
        $("#content").children().each(function (i, a) {
            a.style.display = 'none';
        });
        var name = $Focus.data("url");
        $("#"+name).show();
        page=$("#"+name);
    }
    function init(){
        nodeInit($("#recomend .list"),list.tuijian,"tuijian");
        nodeInit($("#cake .list"),list.cake,"cake");
        nodeInit($("#drink .list"),list.tea,"tea");
        nodeInit($("#donuts .list"),list.donuts,"donuts");
        bind();
    }
    function nodeInit(node,data,type){
        node.html("");
        data.forEach(function(item) {
            node.append(listrender(type,item));
        });
    }
    function listrender(type,data){
        var name=type+"_"+data.id;
        dom_list.bind(name,data);
        return '<li class="" data-name="'+name+'"><img src="'+data.url+'"/><div><span class="price">¥'+data.price+'</span><span>'+data.name+'</span></div></li>';
    }
    function bind(){
        $("#menu li").on("click",function(e){
            var $Focus=$(e.currentTarget);
            $Focus.Focus();
            openContent($Focus);
        })
        $(".poll").on("click",function(e){
            $("#main").hide();
            $(".player").show();
            document.getElementById("video").src="./assess/videoplayback.mp4";
            document.getElementById("video").play();
        })
        $(".control").on("click",function(e){
            $("#main").show();
            $(".player").hide();
            document.getElementById("video").src="";
        })
        $('#content').on('click', '.list li', function(e) {
            var $this=$(this);
            var  data=dom_list.data[$this.data("name")];
            console.log(data);
            details.open(data)
        });
    }
});


