function parseSVGPath(data, factor, rounding) {
    var polygons = [], currentPoint = [0,0], accumulator = [], number = "";
    if(!rounding) rounding = 1.0;

    function round(value) {
        return (rounding == null) ? value : (Math.round(value*rounding)/rounding);
    }

    function getLastCommand() {
        var commands = polygons[polygons.length-1].commands;
        return commands[commands.length-1];
    }

    function mirrorPoint(command) {
        var lastCommand = getLastCommand(),
            x = lastCommand.points[lastCommand.points.length-2],
            y = lastCommand.points[lastCommand.points.length-1];
        if(lastCommand.type == command.type) {
            x = round(2*x-lastCommand.points[lastCommand.points.length-4]);
            y = round(2*y-lastCommand.points[lastCommand.points.length-3]);
        }
        command.points.splice(0, 0, x);
        command.points.splice(1, 0, y);
    }

    function replicate(type, count, callback, interalAdvance) {
        var isRelative = (accumulator[0][0] == accumulator[0][0].toLowerCase()) ? 1 : 0,
            interalAdvance = (isRelative && interalAdvance);
        for(var i = 1; i < accumulator.length; i+=count) {
            var command = {"type":type, "points":[]};
            for(var j = 0; j < count; ++j) {
                var value = round(parseFloat(accumulator[i+j])*factor+currentPoint[j%2]*isRelative);
                if(interalAdvance)
                    currentPoint[j%2] = value;
                command.points.push(value);
            }
            if(callback)
                callback(command);
            polygons[polygons.length-1].commands.push(command);
        }
    }

    function replicateLinear(coordIndex) {
        var isRelative = (accumulator[0][0] == accumulator[0][0].toLowerCase()) ? 1 : 0;
        for(var i = 1; i < accumulator.length; ++i) {
            var command = {"type":"linear", "points":[currentPoint[0], currentPoint[1]]};
            command.points[coordIndex] = round(parseFloat(accumulator[i])*factor+currentPoint[coordIndex]*isRelative);
            polygons[polygons.length-1].commands.push(command);
        }
    }

    function seperate() {
        if(!accumulator.length) return true;
        if(!polygons.length && accumulator[0][0].toLowerCase() != "m") return false;
        switch(accumulator[0][0]) {
            case "m":
            case "M":
                if(accumulator.length % 2 != 1) return false;
                polygons.push({"commands": []});
                replicate("linear", 2, null, true);
            break;
            case "z":
            case "Z":
                if(accumulator.length != 1 || polygons[polygons.length-1].commands.length == 0) return false;
                polygons[polygons.length-1].commands.push({"type":"end"});
            break;
            case "l":
            case "L":
                if(accumulator.length % 2 != 1) return false;
                replicate("linear", 2);
            break;
            case "h":
            case "H":
                if(accumulator.length <= 1) return false;
                replicateLinear(0);
            break;
            case "v":
            case "V":
                if(accumulator.length <= 1) return false;
                replicateLinear(1);
            break;
            case "q":
            case "Q":
                if(accumulator.length % 4 != 1) return false;
                replicate("quadratic", 4);
            break;
            case "t":
            case "T":
                if(accumulator.length % 2 != 1) return false;
                replicate("quadratic", 2, mirrorPoint);
            break;
            case "c":
            case "C":
                if(accumulator.length % 6 != 1) return false;
                replicate("cubic", 6);
            break;
            case "s":
            case "S":
                if(accumulator.length % 4 != 1) return false;
                replicate("cubic", 4, mirrorPoint);
            break;
            case "a":
            case "A":
                if(accumulator.length % 7 != 1) return false;
                for(var i = 1; i < accumulator.length; i += 7) {
                    var isRelative = (accumulator[0][0] == accumulator[0][0].toLowerCase()) ? 1 : 0;
                    var command = {
                        "type":"arc",
                        "width":round(parseFloat(accumulator[i])*factor),
                        "height":round(parseFloat(accumulator[i+1])*factor),
                        "rotation":round(parseFloat(accumulator[i+2])*factor),
                        "flagA":(accumulator[i+3] == "1"),
                        "flagB":(accumulator[i+4] == "1"),
                        "points":[
                            round((parseFloat(accumulator[i+5])*factor+currentPoint[0]*isRelative)),
                            round((parseFloat(accumulator[i+6])*factor+currentPoint[1]*isRelative))
                        ]
                    };
                    polygons[polygons.length-1].commands.push(command);
                }
                console.log("ERROR: SVG arcs are not supported");
                return false;
            default:
                return false;
        }

        var lastCommand = getLastCommand();
        if(lastCommand.points) {
            currentPoint[0] = lastCommand.points[lastCommand.points.length-2];
            currentPoint[1] = lastCommand.points[lastCommand.points.length-1];
        }

        return true;
    }

    for(var i = 0; i < data.length; ++i) {
        switch(data[i]) {
            case ",":
            case " ":
            case "\t":
            case "\n":
                if(number != "") {
                    accumulator.push(number);
                    number = "";
                }
            continue;
            case "-":
                if(number != "")
                    accumulator.push(number);
                number = data[i];
            continue;
            case ".":
            case "e":
            case "0":
            case "1":
            case "2":
            case "3":
            case "4":
            case "5":
            case "6":
            case "7":
            case "8":
            case "9":
                number += data[i];
            continue;
        }
        if(number != "") {
            accumulator.push(number);
            number = "";
        }
        seperate();
        accumulator = [data[i]];
    }
    seperate();

    return polygons;
}

function calculatePointLineDist(p0x, p0y, p1x, p1y, p2x, p2y) {
    var diffX = p2x-p1x, diffY = p2y-p1y;
    return Math.abs((p2y-p1y)*p0x-(p2x-p1x)*p0y+p2x*p1y-p2y*p1x)/Math.sqrt(diffX*diffX+diffY*diffY);
}

function calculateLinearLength(p0x, p0y, p1x, p1y) {
    var diffX = p1x-p0x, diffY = p1y-p0y;
    return Math.sqrt(diffX*diffX+diffY*diffY);
}

// http://math.stackexchange.com/questions/12186/arc-length-of-bézier-curves
function calculateQuardaticLength(dx0, dy0, dx1, dy1, t) {
    var a = (dx1-dx0)*(dx1-dx0)+(dy1-dy0)*(dy1-dy0),
        b = dx0*(dx1-dx0)+dy0*(dy1-dy0),
        c = dx0*dx0+dy0*dy0,
        d = a*c-b*b;
    return (t+b/a)*Math.sqrt(c+2*b*t+a*t*t)+(d/Math.sqrt(a*a*a))*Math.asinh((a*t+b)/Math.sqrt(d));
}

// http://www.circuitwizard.de/metapost/arclength.pdf
function calculateCubicLength(dx0, dy0, dx1, dy1, dx2, dy2, t) {

}

function calculateLineIntersection(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, intersection) {
    var diffAx = p1x-p0x, diffAy = p1y-p0y,
        diffBx = p3x-p2x, diffBy = p3y-p2y,
        diffCx = p0x-p2x, diffCy = p0y-p2y,
        denominator = diffAx*diffBy-diffBx*diffAy;
    if(denominator == 0.0) {
        if(calculatePointLineDist(p1x, p1y, p2x, p2y, p3x, p3y) > 0.0)
            return false;
        //TODO: On same line
        return true;
    }
    denominator = 1.0/denominator;
    var s = (diffBx*diffCy-diffBy*diffCx)*denominator,
        t = (diffAx*diffCy-diffAy*diffCx)*denominator;
    if(intersection && intersection.length == 2) {
        intersection[0] = p0x+s*diffAx;
        intersection[1] = p0y+s*diffAy;
    }
    return (s >= 0 && s <= 1 && t >= 0 && t <= 1);
}

function iteratePolygon(polygon, start, end, callback) {
    for(var i = start; i < polygon.points.length-end; i += 2) {
        var p0x, p0y, p1x = polygon.points[i], p1y = polygon.points[i+1];
        if(i == 0) {
            p0x = polygon.points[polygon.points.length-2];
            p0y = polygon.points[polygon.points.length-1];
        }else{
            p0x = polygon.points[i-2];
            p0y = polygon.points[i-1];
        }
        callback(i, p0x, p0y, p1x, p1y);
    }
}

function calculatePolygonSelfIntersection(polygon) {
    var result = [], intersection = [0,0];
    iteratePolygon(polygon, 0, 4, function(i, p0x, p0y, p1x, p1y) {
        var endJ = polygon.points.length;
        if(i == 0) endJ -= 2;
        for(var j = i+4; j < endJ; j += 2)
            if(calculateLineIntersection(p0x, p0y, p1x, p1y,
                polygon.points[j-2], polygon.points[j-1], polygon.points[j], polygon.points[j+1], intersection))
                result.push({"indexA":i, "indexB":j, "point":intersection});
    });
    return result;
}

function calculatePolygonsIntersection(polygonA, polygonB) {
    var result = [], intersection = [0,0];
    if(polygonA.boundingBox[0] < polygonB.boundingBox[2] &&
       polygonA.boundingBox[2] > polygonB.boundingBox[0] &&
       polygonA.boundingBox[1] < polygonB.boundingBox[3] &&
       polygonA.boundingBox[3] > polygonB.boundingBox[1])
        iteratePolygon(polygonA, 0, 0, function(i, p0x, p0y, p1x, p1y) {
            iteratePolygon(polygonB, 0, 0, function(j, p2x, p2y, p3x, p3y) {
                if(calculateLineIntersection(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y))
                    result.push({"indexA":i, "indexB":j, "point":intersection});
            });
        });
    return result;
}

function iteratePolygonTree(polygon, callback) {
    callback(polygon);
    for(var i in polygon.children) {
        callback(polygon.children[i]);
        iteratePolygonTree(polygon.children[i], callback);
    }
}

function isPointInsidePolygon(polygon, pointX, pointY) {
    if(pointX < polygon.boundingBox[0] || pointX > polygon.boundingBox[2] ||
       pointY < polygon.boundingBox[1] || pointY > polygon.boundingBox[3])
       return false;
    var intersections = 0;
    for(var j = 2; j < polygon.points.length; j += 2) {
        var p0x = polygon.points[j-2], p0y = polygon.points[j-1],
            p1x = polygon.points[j], p1y = polygon.points[j+1];
        if((p0x >= pointX || p1x >= pointX) &&
           (p0y >= pointY || p1y >= pointY) &&
           (p0y <= pointY || p1y <= pointY) &&
           (p0x == p1x || p0x+(pointY-p0y)/(p1y-p0y)*(p1x-p0x) >= pointX))
            ++intersections;
    }
    return intersections%2 == 1;
}

function postProcessPath(polygons) {
    var tStep = 0.1, minDist = 10.0, maxCircleVaricance = 0.001;
    for(var i in polygons) {
        var polygon = polygons[i];
        polygon.children = [];

        //Calculate polygon
        polygon.points = [];
        for(var j in polygon.commands) {
            var command = polygon.commands[j], lastPoint;
            if(j > 0) {
                lastPoint = polygon.commands[j-1].points;
                lastPoint = lastPoint.slice(lastPoint.length-2, lastPoint.length);
            }

            //TODO: Add auto resolution
            switch(command.type) {
                case "linear":
                    polygon.points.push(command.points[0]);
                    polygon.points.push(command.points[1]);
                break;
                case "quadratic":
                    var p0x = lastPoint[0], p0y = lastPoint[1],
                        p1x = command.points[0], p1y = command.points[1],
                        p2x = command.points[2], p2y = command.points[3];
                    for(var t = tStep; t <= 1; t += tStep) {
                        var u = 1-t, a = u*u, b = 2*u*t, c = t*t;
                        polygon.points.push(p0x*a+p1x*b+p2x*c);
                        polygon.points.push(p0y*a+p1y*b+p2y*c);
                    }
                break;
                case "cubic":
                    var p0x = lastPoint[0], p0y = lastPoint[1],
                        p1x = command.points[0], p1y = command.points[1],
                        p2x = command.points[2], p2y = command.points[3],
                        p3x = command.points[4], p3y = command.points[5];
                    for(var t = tStep; t <= 1; t += tStep) {
                        var u = 1-t, a = u*u*u, b = 3*u*u*t, c = 3*u*t*t, d = t*t*t;
                        polygon.points.push(p0x*a+p1x*b+p2x*c+p3x*d);
                        polygon.points.push(p0y*a+p1y*b+p2y*c+p3y*d);
                    }
                break;
            }
        }

        //Check if at least one triangle
        if(polygon.points.length < 6)
            throw {"type":"NoArea", "data":polygon};

        //Merge start and end point
        if(calculateLinearLength(polygon.points[0], polygon.points[1],
            polygon.points[polygon.points.length-2], polygon.points[polygon.points.length-1]) < minDist) {
            polygon.points.splice(polygon.points.length-2, 2);
        }

        //Check for self intersection
        var intersection = calculatePolygonSelfIntersection(polygon);
        if(intersection.length > 0) {
            intersection.polygon = polygon;
            throw {"type":"SelfIntersection", "data":intersection};
        }

        //Find and eliminate redundant lines
        var p0x = polygon.points[polygon.points.length-4], p0y = polygon.points[polygon.points.length-3],
            p1x = polygon.points[polygon.points.length-2], p1y = polygon.points[polygon.points.length-1];
        for(var j = 0; j < polygon.points.length; j += 2) {
            var p2x = polygon.points[j], p2y = polygon.points[j+1];

            var diffAx = p1x-p0x, diffAy = p1y-p0y,
                diffBx = p2x-p1x, diffBy = p2y-p1y,
                aux = false;
            if(diffAx == 0.0)
                aux = (diffBx == 0.0);
            else
                aux = (Math.abs(diffAy/diffAx-diffBy/diffBx) < 0.001);

            if(aux) {
                /*var diffA = Math.sqrt(diffAx*diffAx+diffAy*diffAy),
                    diffB = Math.sqrt(diffBx*diffBx+diffBy*diffBy),
                    diffCx = p2x-p0x, diffCy = p2y-p0y,
                    diffC = Math.sqrt(diffCx*diffCx+diffCy*diffCy);
                if(diffA+diffB > diffC+0.001)
                    throw {"type":"SpikeEdges", "data":{"polygon":polygon, "index":j}};*/

                if(j > 0)
                    polygon.points.splice(j-2, 2);
                else
                    polygon.points.splice(polygon.points.length-2, 2);
            }
            p0x = p1x; p0y = p1y;
            p1x = p2x; p1y = p2y;
        }

        //Claculate Direction, Area, CenterOfMass and BoundingBox
        polygon.signedArea = 0;
        polygon.centerOfMass = [0,0];
        polygon.boundingBox = [polygon.points[0], polygon.points[1], polygon.points[0], polygon.points[1]];
        p1x = polygon.points[polygon.points.length-2];
        p1y = polygon.points[polygon.points.length-1];
        for(var j = 0; j < polygon.points.length; j += 2) {
            var p2x = polygon.points[j], p2y = polygon.points[j+1];

            polygon.signedArea += p1x*p2y-p2x*p1y;

            var factor = (p1x*p2y-p2x*p1y);
            polygon.centerOfMass[0] += (p1x+p2x)*factor;
            polygon.centerOfMass[1] += (p1y+p2y)*factor;

            if(p2x < polygon.boundingBox[0])
                polygon.boundingBox[0] = p2x;
            else if(p2x > polygon.boundingBox[2])
                polygon.boundingBox[2] = p2x;

            if(p2y < polygon.boundingBox[1])
                polygon.boundingBox[1] = p2y;
            else if(p2y > polygon.boundingBox[3])
                polygon.boundingBox[3] = p2y;

            p1x = p2x;
            p1y = p2y;
        }
        polygon.direction = (polygon.signedArea > 0.0) ? "CW" : "CCW";
        polygon.signedArea *= 0.5;
        polygon.centerOfMass[0] *= 1.0/(6*polygon.signedArea);
        polygon.centerOfMass[1] *= 1.0/(6*polygon.signedArea);

        //Circle detection
        var distances = [], avgDistance = 0, variance = 0;
        for(var j = 0; j < polygon.points.length; j += 2) {
            var distance = calculateLinearLength(polygon.centerOfMass[0], polygon.centerOfMass[1], polygon.points[j], polygon.points[j+1]);
            avgDistance += distance;
            distances.push(distance);
        }
        avgDistance /= polygon.points.length/2;
        for(var j in distances) {
            var diff = distances[j]-avgDistance;
            variance += diff*diff;
        }
        variance /= polygon.points.length/2-1;
        if(variance <= maxCircleVaricance)
            polygon.radius = avgDistance;
    }

    //Check intersections
    for(var j = 0; j < polygons.length; ++j) {
        var polygonA = polygons[j];
        for(var j = 0; j < polygons.length; ++j) {
            var polygonB = polygons[j];
            intersection = calculatePolygonsIntersection(polygonA, polygonB);
            if(polygonA != polygonB && intersection.length > 0) {
                intersection.polygonA = polygonA;
                intersection.polygonB = polygonB;
                throw {"type":"ParaIntersection", "data":intersection};
            }
        }
    }

    //Build bollean geometry tree
    for(var i in polygons)
        iteratePolygonTree(polygons[i], function(outerPolygon) {
            for(var j = 0; j < polygons.length; ++j) {
                var innerPolygon = polygons[j];
                if(innerPolygon != outerPolygon && isPointInsidePolygon(outerPolygon, innerPolygon.points[0], innerPolygon.points[1])) {
                    outerPolygon.children.push(innerPolygon);
                    polygons.splice(j--, 1);
                }
            }
        });

    console.log(JSON.stringify(polygons, null, 4));
}
