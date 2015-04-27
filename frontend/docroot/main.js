var canvas, ctx;

function init() {
    var canvasWidth = canvasHeight = 1024,
        scaleFactor = (window.devicePixelRatio) ? window.devicePixelRatio : 1;
    canvas = document.getElementById("canvas");

    document.addEventListener("dragover", function(evt) {
        var files = evt.dataTransfer.files;
        evt.stopPropagation();
        evt.preventDefault();
        evt.dataTransfer.dropEffect = "copy";
    }, false);

    document.addEventListener("drop", function(evt) {
        evt.stopPropagation();
        evt.preventDefault();
        var files = evt.dataTransfer.files;
        if(files.length != 1 || files[0].name.substr(files[0].name.length-4) != ".svg")
            throw "No SVG file";
        var reader = new FileReader();
        reader.onload = function(result) {
            parseSVGFile(result.target.result);
            render(0, 2);
        };
        reader.readAsText(files[0]);
    }, false);
}

function render(offset, radius) {
    while(canvas.firstChild)
        canvas.removeChild(canvas.firstChild);

    for(var i in workpiece)
        traversePolygonTree(workpiece[i], function(polygon, depth) {
            polygon = generateOutline(polygon, (depth%2) ? -offset : offset);
            var data = "";
            /*for(var j in polygon.commands) {
                var command = polygon.commands[j];
                switch(command.type) {
                    case "linear":
                    if(j == 0)
                        data += "M"+command.points[0]+","+command.points[1];
                    else
                        data += "L"+command.points[0]+","+command.points[1];
                    break;
                    case "quadratic":
                    data += "Q"+command.points[0]+","+command.points[1]+","+command.points[2]+","+command.points[3];
                    break;
                    case "cubic":
                    data += "C"+command.points[0]+","+command.points[1]+","+command.points[2]+","+command.points[3]+","+command.points[4]+","+command.points[5];
                    break;
                }
            }*/

            data += "M"+polygon.points[0]+","+polygon.points[1];
            for(var j = 2; j < polygon.points.length; j += 2)
                data += "L"+polygon.points[j]+","+polygon.points[j+1];
            data += "z";

            /*var pr = 1;
            for(var j = 0; j < polygon.points.length; j += 2)
                data += "M"+polygon.points[j]+","+(polygon.points[j+1]+pr)+"a"+pr+","+pr+",0,0,0,0,"+pr*2+"a"+pr+","+pr+",0,0,0,0,-"+pr*2;*/

            var element = document.createElementNS(canvas.getAttribute("xmlns"), "path");
            canvas.appendChild(element);
            element.setAttribute("fill", "none");
            element.setAttribute("stroke", (polygon.intersections) ? "red" : "black");
            element.setAttribute("stroke-linejoin", "round");
            element.setAttribute("stroke-width", 1);
            element.setAttribute("d", data);
            element.onmouseover = function() {
                element.setAttribute("stroke", "cyan");
            };
            element.onmouseout = function() {
                element.setAttribute("stroke", (polygon.intersections) ? "red" : "black");
            };
        });
}
