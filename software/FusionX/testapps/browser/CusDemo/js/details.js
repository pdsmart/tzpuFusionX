define('details',['jquery'], function() {
    
    var details={
        open:function(data){
            this.init(data);
            $(".details").show();
        },
        init:function(data){
            var node=$(".details_item");
            node.html("");
            node.append("<div class='icon'><img src='"+data.url+"'><h2>"+data.name+"</h2></div>");
            node.append("<div class='describe'><div>产品描述</div><div class='price'>¥"+data.price+"</div><p>"+data.describe+"</p></div>");
            node.append(initState(data.state));
        }
    };
    function initState(data){
        var html=$("<div class='state'></div>");
        data.forEach(function(item) {
            var div=$("<div></div>");
            div.append("<div>"+item.name+"</div>");
            var ul=$("<div></div>");
            item.data.forEach(function(s){
                ul.append("<li>"+s+"</li>");
            })
            div.append(ul);
            html.append(div);
        });
        return html;
    }
    function bind(){
        $(".details_content .close").on("click",function(e){
            $(".details").hide();
        })
        $(".details_content").on("click",'.state li',function(e){
           $(this).parent().find(".active").length>0?$(this).parent().find(".active").removeClass("active"):'';
           $(this).addClass("active");
        })
    }
    bind();
    return details;
});