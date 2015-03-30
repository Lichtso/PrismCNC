var accuracyDistance = 2.0, maxCircleVaricance = 0.001;;

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
            if(!interalAdvance)
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
        if(!accumulator.length) return;
        if(!polygons.length && accumulator[0][0].toLowerCase() != "m")
            throw {"type":"Invalid", "data":"SVG path command"};
        switch(accumulator[0][0]) {
            case "m":
            case "M":
                if(accumulator.length % 2 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                polygons.push({"commands": []});
                replicate("linear", 2, null, true);
            break;
            case "z":
            case "Z":
                if(accumulator.length != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                //if(polygons[polygons.length-1].commands.length < 3)
                //    throw {"type":"Invalid", "data":"SVG path command count"};
                //polygons[polygons.length-1].commands.push({"type":"end"});
            break;
            case "l":
            case "L":
                if(accumulator.length % 2 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicate("linear", 2);
            break;
            case "h":
            case "H":
                if(accumulator.length <= 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicateLinear(0);
            break;
            case "v":
            case "V":
                if(accumulator.length <= 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicateLinear(1);
            break;
            case "q":
            case "Q":
                if(accumulator.length % 4 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicate("quadratic", 4);
            break;
            case "t":
            case "T":
                if(accumulator.length % 2 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicate("quadratic", 2, mirrorPoint);
            break;
            case "c":
            case "C":
                if(accumulator.length % 6 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicate("cubic", 6);
            break;
            case "s":
            case "S":
                if(accumulator.length % 4 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
                replicate("cubic", 4, mirrorPoint);
            break;
            case "a":
            case "A":
                if(accumulator.length % 7 != 1)
                    throw {"type":"Invalid", "data":"SVG path argument count"};
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
                throw {"type":"NotSupported", "data":"SVG path arc"};
            default:
                throw {"type":"Invalid", "data":"SVG path command"};
        }

        var lastCommand = getLastCommand();
        if(lastCommand.points) {
            currentPoint[0] = lastCommand.points[lastCommand.points.length-2];
            currentPoint[1] = lastCommand.points[lastCommand.points.length-1];
        }
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
    return Math.abs(diffY*p0x-diffX*p0y+p2x*p1y-p2y*p1x)/Math.sqrt(diffX*diffX+diffY*diffY);
}

function calculateLineLength(p0x, p0y, p1x, p1y) {
    var diffX = p1x-p0x, diffY = p1y-p0y;
    return Math.sqrt(diffX*diffX+diffY*diffY);
}

function calculateLineIntersection(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, intersection) {
    var diffAx = p1x-p0x, diffAy = p1y-p0y,
        diffBx = p3x-p2x, diffBy = p3y-p2y,
        diffCx = p0x-p2x, diffCy = p0y-p2y,
        s, t, denominator = diffAx*diffBy-diffBx*diffAy;

    if(denominator == 0.0) {
        if(Math.abs(diffBy*p1x-diffBx*p1y+p3x*p2y-p3y*p2x) > 0.0)
            return false;

        if(diffBx != 0.0) {
            t = diffCx/diffBx;
        }else if(diffBy != 0.0) {
            t = diffCy/diffBy;
        }else{
            intersection[0] = (p0x+p1x+p2x+p3x)*0.5;
            intersection[1] = (p0y+p1y+p2y+p3y)*0.5;
            if(diffAx != 0.0) {
                s = diffCx/diffAx;
                return (s >= 0.0 && s <= 1.0);
            }else if(diffAy != 0.0) {
                s = diffCy/diffAy;
                return (s >= 0.0 && s <= 1.0);
            }else
                return (p0x == p2x && p0y == p2y);
        }

        if(t >= 0.0 && t <= 1.0) {
            intersection[0] = p0x;
            intersection[1] = p0y;
            return true;
        }

        diffCx = p1x-p2x;
        diffCy = p1y-p2y;
        t = (diffBx != 0.0) ? diffCx/diffBx : diffCy/diffBy;

        if(t >= 0.0 && t <= 1.0) {
            intersection[0] = p1x;
            intersection[1] = p1y;
            return true;
        }

        intersection[0] = (p0x+p1x+p2x+p3x)*0.5;
        intersection[1] = (p0y+p1y+p2y+p3y)*0.5;
        return false;
    }

    denominator = 1.0/denominator;
    s = (diffBx*diffCy-diffBy*diffCx)*denominator;
    t = (diffAx*diffCy-diffAy*diffCx)*denominator;
    intersection[0] = p0x+s*diffAx;
    intersection[1] = p0y+s*diffAy;
    return (s >= 0 && s <= 1 && t >= 0 && t <= 1);
}

function iteratePoints(points, start, end, callback) {
    for(var i = start; i < points.length-end; i += 2) {
        var p0x, p0y, p1x = points[i], p1y = points[i+1];
        if(i == 0) {
            p0x = points[points.length-2];
            p0y = points[points.length-1];
        }else{
            p0x = points[i-2];
            p0y = points[i-1];
        }
        callback(i, p0x, p0y, p1x, p1y);
    }
}

function calculatePolygonsIntersection(polygonA, polygonB) {
    var result = [], intersection = [0,0];
    if(polygonA.boundingBox[0] < polygonB.boundingBox[2] &&
       polygonA.boundingBox[2] > polygonB.boundingBox[0] &&
       polygonA.boundingBox[1] < polygonB.boundingBox[3] &&
       polygonA.boundingBox[3] > polygonB.boundingBox[1])
        iteratePoints(polygonA.points, 0, 0, function(i, p0x, p0y, p1x, p1y) {
           iteratePoints(polygonB.points, 0, 0, function(j, p2x, p2y, p3x, p3y) {
                if(calculateLineIntersection(p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, intersection))
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

function generateCurve(points, curve, offset, tStep, callback) {
    var point = [0,0], lastX = curve[0], lastY = curve[1], length = 0.0;
    for(var t = tStep; t <= 1+tStep*0.5; t += tStep) {
        var u = 1-t;
        callback(point, curve, offset, t, u, t*t, u*u);
        length += calculateLineLength(lastX, lastY, point[0], point[1]);
        lastX = point[0];
        lastY = point[1];
    }
    tStep = 1.0/Math.round(length/accuracyDistance);
    if(tStep >= 0.5) tStep = 0.5;
    for(var t = (offset == 0.0) ? tStep : 0.0; t <= 1+tStep*0.5; t += tStep) {
        var u = 1-t;
        callback(point, curve, offset, t, u, t*t, u*u);
        points = points.concat(point);
    }
    return points;
}

function generateOutline(polygon, offset) {
    var points = [], tStep = 0.1;
    offset = (polygon.signedArea > 0.0) ? -offset : offset;

    for(var j in polygon.commands) {
        var command = polygon.commands[j], pointIndex = points.length, lastPoint;
        lastPoint = polygon.commands[(j > 0) ? j-1 : polygon.commands.length-1].points;
        lastPoint = lastPoint.slice(lastPoint.length-2, lastPoint.length);

        switch(command.type) {
            case "linear":
                // (1-t)*p0+t*p1
                // p1-p0
                var dx = command.points[0]-lastPoint[0],
                    dy = command.points[1]-lastPoint[1],
                    factor = Math.sqrt(dx*dx+dy*dy);
                if(factor >= accuracyDistance) {
                    factor = offset/factor;
                    if(offset != 0.0) {
                        points.push(lastPoint[0]-dy*factor);
                        points.push(lastPoint[1]+dx*factor);
                    }
                    points.push(command.points[0]-dy*factor);
                    points.push(command.points[1]+dx*factor);
                }
            break;
            case "quadratic":
                // (1-t)^2*p0+2*(1-t)*t*p1+t^2*p2
                // 2*(p0*(t-1)-2*p1*t+p1+p2*t)
                var curve = [lastPoint[0], lastPoint[1]].concat(command.points);
                points = generateCurve(points, curve, offset, tStep, function(point, curve, offset, t, u, t2, u2) {
                    var a = t-1, b = 1-2*t, c = 2*u*t;
                    var dx = a*curve[0]+b*curve[2]+t*curve[4],
                        dy = a*curve[1]+b*curve[3]+t*curve[5],
                        factor = offset/Math.sqrt(dx*dx+dy*dy);
                    point[0] = u2*curve[0]+c*curve[2]+t2*curve[4]-factor*dy;
                    point[1] = u2*curve[1]+c*curve[3]+t2*curve[5]+factor*dx;
                });
            break;
            case "cubic":
                // (1-t)^3*p0+3*(1-t)^2*t*p1+3*(1-t)*t^2*p2+t^3*p3
                // -3*(p0*(t-1)^2+p1*(-3*t^2 +4*t -1) +t*(3*p2*t -2*p2 -p3*t))
                var curve = [lastPoint[0], lastPoint[1]].concat(command.points);
                points = generateCurve(points, curve, offset, tStep, function(point, curve, offset, t, u, t2, u2) {
                    var a = t2-2*t+1, b = 4*t-3*t2-1, c = 3*t2-2*t, d = u2*u, e = 3*u2*t, f = 3*u*t2, g = t*t2;
                    var dx = a*curve[0]+b*curve[2]+c*curve[4]-t2*curve[6],
                        dy = a*curve[1]+b*curve[3]+c*curve[5]-t2*curve[7],
                        factor = offset/Math.sqrt(dx*dx+dy*dy);
                    point[0] = d*curve[0]+e*curve[2]+f*curve[4]+g*curve[6]+factor*dy;
                    point[1] = d*curve[1]+e*curve[3]+f*curve[5]+g*curve[7]-factor*dx;
                });
            break;
        }

        if(offset != 0.0 && pointIndex >= 4) {
            var intersection = [0,0];
            calculateLineIntersection(
                points[pointIndex-4], points[pointIndex-3], points[pointIndex-2], points[pointIndex-1],
                points[pointIndex], points[pointIndex+1], points[pointIndex+2], points[pointIndex+3],
                intersection);
            points[pointIndex] = intersection[0];
            points[pointIndex+1] = intersection[1];
            points.splice(pointIndex-2, 2);
        }
    }

    if(offset != 0.0) {
        var intersection = [0,0];
        calculateLineIntersection(
            points[0], points[1], points[2], points[3],
            points[points.length-4], points[points.length-3], points[points.length-2], points[points.length-1],
            intersection);
        points[0] = intersection[0];
        points[1] = intersection[1];
        points.splice(points.length-2, 2);
    }

    //Check if at least one triangle
    if(points.length < 6)
        throw {"type":"NoArea", "data":polygon};

    //Find and eliminate redundant lines
    for(var j = 0; j < points.length; j += 2) {
        var p0x, p0y, p1x, p1y;

        if(j >= 4) {
            p0x = points[j-4];
            p0y = points[j-3];
            p1x = points[j-2];
            p1y = points[j-1];
        }else if(j >= 2) {
            p0x = points[points.length-2];
            p0y = points[points.length-1];
            p1x = points[j-2];
            p1y = points[j-1];
        }else{
            p0x = points[points.length-4];
            p0y = points[points.length-3];
            p1x = points[points.length-2];
            p1y = points[points.length-1];
        }

        var p2x = points[j], p2y = points[j+1];

        var diffAx = p1x-p0x, diffAy = p1y-p0y,
            diffBx = p2x-p1x, diffBy = p2y-p1y,
            aux;
        if(diffAx == 0.0)
            aux = (diffAy == 0.0 || diffBx == 0.0);
        else if(diffBx == 0.0)
            aux = (diffAx == 0.0 || diffBy == 0.0);
        else
            aux = (Math.abs(diffAy/diffAx-diffBy/diffBx) < 0.001);

        if(aux) {
            var diffA = Math.sqrt(diffAx*diffAx+diffAy*diffAy),
                diffB = Math.sqrt(diffBx*diffBx+diffBy*diffBy),
                diffCx = p2x-p0x, diffCy = p2y-p0y,
                diffC = Math.sqrt(diffCx*diffCx+diffCy*diffCy);
            if(diffA+diffB > diffC+0.001)
                throw {"type":"SpikeEdges", "data":{"polygon":polygon, "index":j}};

            if(j > 0)
                points.splice(j-2, 2);
            else
                points.splice(points.length-2, 2);
            j -= 2;
        }
    }

    //TODO: Check for self intersection
    iteratePoints(points, 0, 4, function(i, p0x, p0y, p1x, p1y) {
        var endJ = points.length, intersection = [0,0];
        if(i == 0) endJ -= 2;
        for(var j = i+4; j < endJ; j += 2)
            if(calculateLineIntersection(p0x, p0y, p1x, p1y,
                points[j-2], points[j-1], points[j], points[j+1], intersection)) {

                }
    });

    return points;
}

function postProcessPath(polygons) {
    for(var i in polygons) {
        var polygon = polygons[i];
        polygon.children = [];

        //Calculate outline
        polygon.points = generateOutline(polygon, 0.0);

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
            var distance = calculateLineLength(polygon.centerOfMass[0], polygon.centerOfMass[1], polygon.points[j], polygon.points[j+1]);
            avgDistance += distance;
            distances.push(distance);
        }
        avgDistance /= polygon.points.length/2;
        for(var j in distances) {
            var diff = distances[j]-avgDistance;
            variance += diff*diff;
        }
        variance /= polygon.points.length/2-1;
        if(variance <= maxCircleVaricance) {
            polygon.radius = avgDistance;
            polygon.radiusVariance = variance;
        }
    }

    //Check intersections
    for(var j = 0; j < polygons.length; ++j) {
        var polygonA = polygons[j];
        for(var j = 0; j < polygons.length; ++j) {
            var polygonB = polygons[j];
            if(polygonA == polygonB) continue;
            intersection = calculatePolygonsIntersection(polygonA, polygonB);
            if(intersection.length > 0) {
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
