var canvas, ctx, polygons = [];
function init() {
    var canvasWidth = canvasHeight = 1024,
        scaleFactor = (window.devicePixelRatio) ? window.devicePixelRatio : 1;
    canvas = document.getElementById("canvas");
    /*ctx = canvas.getContext("2d");
    canvas.style.width = canvasWidth+"px";
    canvas.style.height = canvasHeight+"px";
    canvas.setAttribute("width", canvasWidth*scaleFactor);
    canvas.setAttribute("height", canvasHeight*scaleFactor);
    scaleFactor = 8;
    ctx.transform(scaleFactor, 0, 0, scaleFactor, 200, 200);*/

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
            parseSVGFile(polygons, result.target.result);
            render(0, 2);
        };
        reader.readAsText(files[0]);
    }, false);
}

function render(offset, radius) {
    while(canvas.firstChild)
        canvas.removeChild(canvas.firstChild);

    for(var i in polygons) {
        var polygon = polygons[i];
        traversePolygonTree(polygon, function(polygon) {
            polygon = generateOutline(polygon, offset);
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
            element.setAttribute("stroke", "black");
            element.setAttribute("stroke-linejoin", "round");
            element.setAttribute("stroke-width", radius*2);
            element.setAttribute("d", data);
        });
    }

    /*
    var wireFrame = true;
    //ctx.lineWidth = radius*2;
    ctx.lineCap = "round";
    ctx.lineJoin = 'round';
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    //for(var i in polygons)
        //traversePolygonTree(polygons[i], function(polygon) {
        var polygon = generateOutline(polygons[0], offset);
            for(var j = 0; j < polygon.points.length; j += 2) {
                if(wireFrame) {
                    ctx.beginPath();
                    if(j)
                        ctx.moveTo(polygon.points[j-2], polygon.points[j-1]);
                    else
                        ctx.moveTo(polygon.points[polygon.points.length-2], polygon.points[polygon.points.length-1]);
                    ctx.lineTo(polygon.points[j], polygon.points[j+1]);
                }else{
                    ctx.fillRect(polygon.points[j]-0.5, polygon.points[j+1]-0.5, 1, 1);
                    ctx.fillText(j/2, polygon.points[j]-0.5, polygon.points[j+1]);
                }
                if(wireFrame) {
                    //ctx.closePath();
                    ctx.strokeStyle = (j % 4 == 0) ? "#0044ff" : "#00aaff";
                    ctx.stroke();
                }
            }
            //ctx.fillRect(polygon.centerOfMass[0], polygon.centerOfMass[1], 1, 1);
        //});
    */
    //M142.663,28.346L36.85,28.347c-4.677,0-8.504,3.827-8.504,8.504V48.19c0,4.677,3.827,8.504,8.504,8.504h56.693c1.174,0,2.237-0.476,3.007-1.245l10.995-14.517c0.769-0.769,1.832-1.245,3.007-1.245h9.095c1.251,0,2.312,0.81,2.688,1.934l-0.001-0.001c1.471,5.42,6.426,9.406,12.312,9.406 c7.045,0,12.756-5.711,12.756-12.756C147.402,34.261,145.554,30.685,142.663,28.346zM36.85,42.52c-3.131,0-5.669-2.538-5.669-5.669s2.538-5.669,5.669-5.669s5.669,2.538,5.669,5.669S39.981,42.52,36.85,42.52z M59.527,53.15c-1.174,0-2.126-0.952-2.126-2.126 s0.952-2.126,2.126-2.126c1.174,0,2.126,0.952,2.126,2.126S60.702,53.15,59.527,53.15z M87.874,53.15 c-1.174,0-2.126-0.952-2.126-2.126s0.952-2.126,2.126-2.126c1.174,0,2.126,0.952,2.126,2.126S89.048,53.15,87.874,53.15zM134.646,45.355c-3.914,0-7.087-3.173-7.087-7.087c0-3.914,3.173-7.087,7.087-7.087c3.914,0,7.087,3.173,7.087,7.087C141.732,42.182,138.559,45.355,134.646,45.355z
}
