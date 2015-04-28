function init() {
    var svgCanvas = document.getElementById("svgCanvas"),
        svgDropZone = document.getElementById("svgDropZone"),
        svgLoadingIndicator = document.getElementById("svgLoadingIndicator");

    document.body.addEventListener("dragover", function(evt) {
        evt.stopPropagation();
        evt.preventDefault();
    });

    document.body.addEventListener("drop", function(evt) {
        evt.stopPropagation();
        evt.preventDefault();
    });

    svgDropZone.addEventListener("dragover", function(evt) {
        evt.stopPropagation();
        evt.preventDefault();
        evt.dataTransfer.dropEffect = "copy";
        svgDropZone.childNodes[1].setAttribute("class", "file outline huge icon");
    });

    svgDropZone.addEventListener("dragleave", function(evt) {
        svgDropZone.childNodes[1].setAttribute("class", "file huge icon");
    });

    svgDropZone.addEventListener("drop", function(evt) {
        evt.stopPropagation();
        evt.preventDefault();
        var files = evt.dataTransfer.files;
        if(files.length != 1 || files[0].name.substr(files[0].name.length-4) != ".svg")
            throw "No SVG file";
        var reader = new FileReader();
        svgCanvas.style.display = "initial";
        svgDropZone.style.display = "none";
        svgLoadingIndicator.setAttribute("class", "ui dimmer active");
        reader.onload = function(result) {
            parseSVGFile(result.target.result);
            render(0, 2);
            svgLoadingIndicator.setAttribute("class", "ui dimmer");
        };
        reader.readAsText(files[0]);
    });

    svgCanvas.parentNode.addEventListener("click", function(evt) {
        svgCanvas.style.display = "none";
        svgDropZone.style.display = "block";
        svgDropZone.childNodes[1].setAttribute("class", "file huge icon");
        svgLoadingIndicator.setAttribute("class", "ui dimmer");
    });

    $('.ui.dropdown').dropdown();
    $('.left.sidebar .item').tab();
    $('.right.attached.fixed.button')[0].onclick = function() {
        $('.left.sidebar').sidebar('toggle');
    };
}

function render(offset, radius) {
    var svgCanvas = document.getElementById("svgCanvas");
    while(svgCanvas.firstChild)
        svgCanvas.removeChild(svgCanvas.firstChild);

    for(var i in workpiece)
        traversePolygonTree(workpiece[i], function(original, depth) {
            var maxR = (depth == 0) ? offset : 0;
            for(var r = 0; r <= maxR; r += 10) {
                var polygon = generateOutline(original, (depth%2) ? -r : r);

                var data = "";
                data += "M"+polygon.points[0]+","+polygon.points[1];
                for(var j = 2; j < polygon.points.length; j += 2)
                    data += "L"+polygon.points[j]+","+polygon.points[j+1];
                data += "z";

                /*var pr = 1;
                for(var j = 0; j < polygon.points.length; j += 2)
                    data += "M"+polygon.points[j]+","+(polygon.points[j+1]+pr)+"a"+pr+","+pr+",0,0,0,0,"+pr*2+"a"+pr+","+pr+",0,0,0,0,-"+pr*2;*/

                var element = document.createElementNS(svgCanvas.getAttribute("xmlns"), "path");
                svgCanvas.appendChild(element);
                element.setAttribute("d", data);
                element.setAttribute("fill", "none");
                element.setAttribute("stroke", (r == 0) ? "black" : "green");
                element.setAttribute("stroke-linejoin", "round");
                element.setAttribute("stroke-width", 2);
                /*element.onmouseover = function() {
                    element.setAttribute("stroke", "cyan");
                };
                element.onmouseout = function() {
                    element.setAttribute("stroke", (polygon.intersections) ? "red" : "black");
                };*/
            }
        });
}
