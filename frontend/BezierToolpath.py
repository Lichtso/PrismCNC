bl_info = {
    "name": "Bezier Intersection",
    "description": "Bezier Intersection",
    "author": "Alexander Meißner",
    "version": (1, 0),
    "blender": (2, 77, 0),
    "category": "Curve"
}

import bpy
import socket
import msgpack
from mathutils import geometry, Vector, Matrix
from collections import namedtuple
from bpy.props import *

SplineBezierSegement = namedtuple('SplineBezierSegement', 'spline curve beginIndex endIndex params')
AABB = namedtuple('AxisAlignedBoundingBox', 'center dimensions')
Plane = namedtuple('Plane', 'normal distance')
Circle = namedtuple('Circle', 'plane center radius')

def nearestPointOfLines(a, b):
    # https://en.wikipedia.org/wiki/Skew_lines#Nearest_Points
    normal = a.dir.cross(b.dir)
    normalA = a.dir.cross(normal)
    normalB = b.dir.cross(normal)
    origins = b.origin-a.origin
    paramA = origins.dot(normalB)/a.dir.dot(normalB)
    paramB = origins.dot(normalA)/b.dir.dot(normalA)
    # TODO

def planeOfTriangle(a, b, c):
    normal = (b-a).cross(c-b).normalize()
    distance = a.dot(normal)
    return Plane(normal=normal, distance=distance)

def circleOfTriangle(a, b, c):
    # https://en.wikipedia.org/wiki/Circumscribed_circle#Cartesian_coordinates_from_cross-_and_dot-products
    dirBA = a-b
    dirCB = b-c
    dirAC = c-a
    normal = dirBA.cross(dirCB)
    lengthBA = dirBA.length
    lengthCB = dirCB.length
    lengthAC = dirAC.length
    lengthN = normal.length
    lengthN2 = lengthN*lengthN
    factor = -1/(2*lengthN*lengthN)
    alpha = lengthCB*lengthCB*dirBA.dot(dirAC)*factor
    beta = lengthAC*lengthAC*dirBA.dot(dirCB)*factor
    gamma = lengthBA*lengthBA*dirAC.dot(dirCB)*factor
    center = a*alpha+b*beta+c*gamma
    radius = (lengthBA*lengthCB*lengthAC)/(2*lengthN)
    plane = Plane(normal=normal, distance=center.dot(normal))
    return Circle(plane=plane, center=center, radius=radius)

def aabbOfPoints(points):
    min = Vector(points[0])
    max = Vector(points[0])
    for point in points:
        for i in range(0, 3):
            if min[i] > point[i]:
                min[i] = point[i]
            if max[i] < point[i]:
                max[i] = point[i]
    return AABB(center=(max+min)*0.5, dimensions=max-min)

def aabbIntersectionTest(a, b, tollerance=0.0):
    for i in range(0, 3):
        if abs(a.center[i]-b.center[i]) > (a.dimensions[i]+b.dimensions[i]+tollerance):
            return False
    return True

def bezierPointAt(curve, param):
    coParam = 1.0-param
    paramSquared = param*param
    coParamSquared = coParam*coParam
    a = coParam*coParamSquared
    b = 3.0*coParamSquared*param
    c = 3.0*coParam*paramSquared
    d = paramSquared*param
    return a*curve[0]+b*curve[1]+c*curve[2]+d*curve[3]

def bezierTangentAt(curve, param):
    coParam = 1.0-param
    paramSquared = param*param
    coParamSquared = coParam*coParam
    a = coParam*coParam
    b = 2.0*coParam*param
    c = param*param
    return a*(curve[1]-curve[0])+b*(curve[2]-curve[1])+c*(curve[3]-curve[2])

def bezierSubCurveFromTo(curve, fromParam, toParam):
    fromP = bezierPointAt(curve, fromParam)
    fromT = bezierTangentAt(curve, fromParam)
    toP = bezierPointAt(curve, toParam)
    toT = bezierTangentAt(curve, toParam)
    paramDiff = toParam-fromParam
    return [fromP, fromP+fromT*paramDiff, toP-toT*paramDiff, toP]

def bezierSubdivideCurveAt(curve, params):
    newPoints = []
    newPoints.append(curve[0]+(curve[1]-curve[0])*params[0])
    for index in range(0, len(params)):
        point = bezierPointAt(curve, params[index])
        tangent = bezierTangentAt(curve, params[index])
        paramLeft = params[index]
        if index > 0:
            paramLeft -= params[index-1]
        paramRight = -params[index]
        if index == len(params)-1:
            paramRight += 1.0
        else:
            paramRight += params[index+1]
        newPoints.append(point-tangent*paramLeft)
        newPoints.append(point)
        newPoints.append(point+tangent*paramRight)
    newPoints.append(curve[3]-(curve[3]-curve[2])*(1.0-params[-1]))
    return newPoints

def bezierIntersectionBroadPhase(solutions, depth, curveA, curveB, aFrom, aTo, bFrom, bTo):
    if aabbIntersectionTest(aabbOfPoints(bezierSubCurveFromTo(curveA, aFrom, aTo)), aabbOfPoints(bezierSubCurveFromTo(curveB, bFrom, bTo)), 0.001) == False:
        return
    if depth == 0:
        solutions.append([aFrom, aTo, bFrom, bTo])
        return
    depth -= 1
    aMid = (aFrom+aTo)*0.5
    bMid = (bFrom+bTo)*0.5
    bezierIntersectionBroadPhase(solutions, depth, curveA, curveB, aFrom, aMid, bFrom, bMid)
    bezierIntersectionBroadPhase(solutions, depth, curveA, curveB, aFrom, aMid, bMid, bTo)
    bezierIntersectionBroadPhase(solutions, depth, curveA, curveB, aMid, aTo, bFrom, bMid)
    bezierIntersectionBroadPhase(solutions, depth, curveA, curveB, aMid, aTo, bMid, bTo)

def bezierIntersectionNarrowPhase(broadPhase, curveA, curveB):
    aFrom = broadPhase[0]
    aTo = broadPhase[1]
    bFrom = broadPhase[2]
    bTo = broadPhase[3]
    while True:
        aMid = (aFrom+aTo)*0.5
        bMid = (bFrom+bTo)*0.5
        a1 = bezierPointAt(curveA, (aFrom+aMid)*0.5)
        a2 = bezierPointAt(curveA, (aMid+aTo)*0.5)
        b1 = bezierPointAt(curveB, (bFrom+bMid)*0.5)
        b2 = bezierPointAt(curveB, (bMid+bTo)*0.5)
        a1b1Dist = (a1-b1).length
        a2b1Dist = (a2-b1).length
        a1b2Dist = (a1-b2).length
        a2b2Dist = (a2-b2).length
        minDist = min(a1b1Dist, a2b1Dist, a1b2Dist, a2b2Dist)
        if a1b1Dist == minDist:
            aTo = aMid
            bTo = bMid
        elif a2b1Dist == minDist:
            aFrom = aMid
            bTo = bMid
        elif a1b2Dist == minDist:
            aTo = aMid
            bFrom = bMid
        else:
            aFrom = aMid
            bFrom = bMid
        if aFrom == aTo and bFrom == bTo:
            return [aFrom, bFrom, minDist]

def bezierIntersection(curveA, curveB, paramsA, paramsB):
    solutions = []
    bezierIntersectionBroadPhase(solutions, 8, curveA, curveB, 0.0, 1.0, 0.0, 1.0)
    for index in range(0, len(solutions)):
        solutions[index] = bezierIntersectionNarrowPhase(solutions[index], curveA, curveB)

    for index in range(0, len(solutions)):
        for otherIndex in range(0, len(solutions)):
            if solutions[index][2] == float("inf"):
                break
            if index == otherIndex or solutions[otherIndex][2] == float("inf"):
                continue
            diffA = solutions[index][0]-solutions[otherIndex][0]
            diffB = solutions[index][1]-solutions[otherIndex][1]
            if diffA*diffA+diffB*diffB < 0.01:
                if solutions[index][2] < solutions[otherIndex][2]:
                    solutions[otherIndex][2] = float("inf")
                else:
                    solutions[index][2] = float("inf")

    for solution in solutions:
        print(str(solution))
        if solution[2] < 0.001:
            paramsA.append(solution[0])
            paramsB.append(solution[1])

    paramsA.sort()
    paramsB.sort()

def getSelectedBezierSegments(splines):
    segments = []
    for spline in splines:
        for index in range(0, len(spline.bezier_points)):
            point = spline.bezier_points[index]
            if point.select_right_handle:
                nextIndex = (index+1)%len(spline.bezier_points)
                nextPoint = spline.bezier_points[nextIndex]
                if nextPoint.select_left_handle == False:
                    continue
                segments.append(SplineBezierSegement(
                                spline=spline,
                                beginIndex=index,
                                endIndex=nextIndex,
                                curve=[Vector(point.co), Vector(point.handle_right), Vector(nextPoint.handle_left), Vector(nextPoint.co)],
                                params=[]))
            if index > 0:
                point.select_left_handle = False
            point.select_control_point = False
            point.select_right_handle = False
        spline.bezier_points[0].select_left_handle = False
    return segments

def subdivideBezierSegmentAtParams(segment):
    # Blender only allows uniform subdivision. Use this method to subdivide at arbitrary params
    # Everything has to be deselected and segment.params must be sorted in ascending order
    endIndex = segment.endIndex
    begin = segment.spline.bezier_points[segment.beginIndex]
    end = segment.spline.bezier_points[endIndex]
    newPoints = bezierSubdivideCurveAt(segment.curve, segment.params)
    begin.select_right_handle = True
    end.select_left_handle = True
    bpy.ops.curve.subdivide(number_cuts=len(segment.params))
    if endIndex > 0:
        endIndex += len(segment.params)
    begin = segment.spline.bezier_points[segment.beginIndex]
    end = segment.spline.bezier_points[endIndex]
    begin.handle_right_type = "FREE"
    end.handle_left_type = "FREE"
    begin.select_right_handle = False
    end.select_left_handle = False
    begin.handle_right = newPoints[0]
    end.handle_left = newPoints[-1]
    for index in range(0, len(segment.params)):
        newPoint = segment.spline.bezier_points[segment.beginIndex+1+index]
        newPoint.handle_left_type = "FREE"
        newPoint.handle_right_type = "FREE"
        newPoint.select_left_handle = False
        newPoint.select_control_point = False
        newPoint.select_right_handle = False
        newPoint.handle_left = newPoints[index*3+1]
        newPoint.co = newPoints[index*3+2]
        newPoint.handle_right = newPoints[index*3+3]

def subdivideBezierSegmentsAtParams(segments):
    # Segements of the same spline have to be sorted with the higher indecies being subdivided first
    # to prevent the lower ones from shifting the indecies of the higher ones
    groups = {}
    for segment in segments:
        spline = segment.spline
        if (spline in groups) == False:
            groups[spline] = []
        group = groups[spline]
        group.append(segment)
    for spline in groups:
        group = groups[spline]
        group.sort(key=lambda segment: segment.beginIndex, reverse=True)
        for segment in group:
            subdivideBezierSegmentAtParams(segment)

class BezierIntersection(bpy.types.Operator):
    bl_idname = "curve.bezier_intersection"
    bl_label = "Bezier Intersection"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(self, context):
        obj = bpy.context.object
        return obj != None and obj.type == "CURVE" and obj.mode == "EDIT"

    def execute(self, context):
        obj = bpy.context.object
        segments = getSelectedBezierSegments(obj.data.splines)
        if len(segments) == 2:
            bezierIntersection(segments[0].curve, segments[1].curve, segments[0].params, segments[1].params)
            if len(segments[0].params) > 0:
                subdivideBezierSegmentsAtParams(segments)
        else:
            self.report({"ERROR"}, "Invalid selection")
        return {'FINISHED'}

class SendToolpathOperator(bpy.types.Operator):
    bl_idname = "object.send_toolpath"
    bl_label = "Send Toolpath"
    bl_options = {"REGISTER", "PRESET"}

    address = StringProperty(name="IP-Address", default="")
    port = IntProperty(name="Port", default=3823)
    scale = FloatProperty(name="Scale", default=10.0)
    maxSpeed = 3.0

    @classmethod
    def poll(cls, context):
        obj = bpy.context.object
        return obj != None and obj.type == "CURVE" and obj.mode == "EDIT"

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        packer = msgpack.Packer()
        obj = bpy.context.object
        transform = Matrix.Scale(self.scale, 4)*obj.matrix_world

        selectedSpline = None
        for spline in obj.data.splines:
            for index in range(0, len(spline.bezier_points)):
                point = spline.bezier_points[index]
                if point.select_left_handle or point.select_control_point or point.select_right_handle:
                    if selectedSpline != None:
                        self.report({"ERROR"}, "Multiple curves selected")
                        return {"FINISHED"}
                    if spline.use_cyclic_u or spline.use_cyclic_v:
                        self.report({"ERROR"}, "Curve must not be cyclic")
                        return {"FINISHED"}
                    selectedSpline = spline
                    break
        if selectedSpline == None:
            self.report({"ERROR"}, "No curve selected")
            return {"FINISHED"}

        packet = []
        for index in range(0, len(selectedSpline.bezier_points)):
            point = selectedSpline.bezier_points[index]
            vertex = {
                "speed": point.weight_softbody*self.maxSpeed,
                "prev": transform*point.handle_left,
                "pos": transform*point.co,
                "next": transform*point.handle_right
            }
            vertex["prev"] = (-vertex["prev"].x, vertex["prev"].y, vertex["prev"].z)
            vertex["pos"] = (-vertex["pos"].x, vertex["pos"].y, vertex["pos"].z)
            vertex["next"] = (-vertex["next"].x, vertex["next"].y, vertex["next"].z)
            packet.append(vertex)
        try:
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client.settimeout(1)
            client.connect((self.address, int(self.port)))
            client.send(packer.pack({"type": "curve", "vertices": packet}))
            client.close()
        except socket.error as error:
            self.report({"ERROR"}, "Could not connect: "+str(error))
        return {"FINISHED"}

def menu_func(self, context):
    self.layout.operator(BezierIntersection.bl_idname, text="Place vertex at curve intersection")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.VIEW3D_MT_edit_curve_specials.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.VIEW3D_MT_edit_curve_specials.remove(menu_func)
