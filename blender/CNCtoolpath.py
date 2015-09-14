import bpy
from mathutils import *
from math import *
from bpy.props import *
from bpy_extras.object_utils import AddObjectHelper, object_data_add
import bmesh
import socket
import msgpack

bl_info = {
    "name": "CNC Toolpath",
    "description": "CNC mesh to toolpath converter.",
    "author": "Alexander Meißner",
    "version": (1, 0),
    "blender": (2, 75, 0),
    "category": "Export"
}
floatFormat = "{:.4f}"

def ToolpathToJSON(obj, scale, slowSpeed, fastSpeed):
    result = []
    vertices = []

    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    mesh.verts.ensure_lookup_table()
    mesh.faces.ensure_lookup_table()
    if len(mesh.faces) != 1:
        return "Missing start face"

    vertex = None
    for eI, vert in enumerate(mesh.faces[0].verts):
        if len(vert.link_edges) == 3:
            if vertex == None:
                vertex = vert
            else:
                return "Multiple start vertices"
        elif len(vert.link_edges) != 2:
            return "Illegal start face"

    prevVertex = None
    prevEdge = None
    prevSpeed = fastSpeed
    while True:
        if prevVertex != None and len(vertex.link_edges) > 2:
            return "Path forks"
        speed = fastSpeed
        if prevEdge != None:
            if prevEdge.smooth == True:
                speed = fastSpeed
            else:
                speed = slowSpeed
        if speed != prevSpeed:
            result.append({"type": "polygon", "speed":prevSpeed, "vertices":vertices})
            vertices = []
        prevSpeed = speed
        pos = obj.matrix_world*(vertex.co*scale)
        vertices.append((-pos.x, pos.y, pos.z))
        if len(vertex.link_edges) < 2:
            break
        for eI, prevEdge in enumerate(vertex.link_edges):
            vert = prevEdge.other_vert(vertex)
            if vert != prevVertex and len(vert.link_faces) == 0:
                prevVertex = vertex
                vertex = vert
                break
    result.append({"type": "polygon", "speed":prevSpeed, "vertices":vertices})

    mesh.to_mesh(obj.data)
    mesh.free()
    return result

class SendToolpathOperator(bpy.types.Operator):
    bl_idname = "object.send_toolpath"
    bl_label = "Send Toolpath"
    bl_options = {"REGISTER", "PRESET"}

    address = StringProperty(name="IP-Address", default="")
    port = IntProperty(name="Port", default=3823)
    scale = FloatProperty(name="Scale", default=10.0)
    slowSpeed = FloatProperty(name="Slow Speed", default=1.0)
    fastSpeed = FloatProperty(name="Fast Speed", default=3.0)

    @classmethod
    def poll(cls, context):
        return context.active_object != None and context.active_object.mode == "OBJECT"

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        packer = msgpack.Packer()
        result = ToolpathToJSON(context.active_object, self.scale, self.slowSpeed, self.fastSpeed)
        if isinstance(result, str):
            self.report({"ERROR"}, result)
        else:
            try:
                client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                client.settimeout(1)
                client.connect((self.address, int(self.port)))
                for item in result:
                    client.send(packer.pack(item))
                client.close()
            except socket.error as error:
                self.report({"ERROR"}, "Could not connect: "+str(error))
        return {"FINISHED"}



def create_toolpath_object(self, context, verts, speeds, name):
    edges = []
    faces = []
    for i in range(0, len(verts)-1):
        edges.append([i, i+1])

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, edges, faces)
    for i in range(0, len(verts)-1):
        mesh.edges[i].use_edge_sharp = speeds[i]
    mesh.show_edge_sharp = True
    mesh.update()
    from bpy_extras import object_utils
    return object_utils.object_data_add(context, mesh, operator=self)

class RectToolpathOperator(bpy.types.Operator, AddObjectHelper):
    bl_idname = "mesh.toolpath_add_rect"
    bl_label = "Rect Toolpath"
    bl_options = {"REGISTER", "UNDO"}

    track_count = IntProperty(name="Number Tracks", description = "How many tracks", min=1, default=10)
    stride = FloatProperty(name="Stride", description="Distance to edge on the way back", min=0.0, default=0.05)
    toolRadius = FloatProperty(name="Tool Radius", description="Radius of the tool", min=0.0, default=0.15)
    penetration = FloatProperty(name="Penetration", description="How much of the tool radius is used", min=0.0, max=1.0, default=2.0/3.0)
    length = FloatProperty(name="Length", description="Length of one track", default=1.0)

    def execute(self, context):
        vertices = []
        speeds = []
        pitch = -self.toolRadius*self.penetration
        stride = self.toolRadius*0.5

        for i in range(0, self.track_count):
            shift = i*pitch
            vertices.append(Vector((shift, 0.0, 0.0)))
            vertices.append(Vector((shift, self.length, 0.0)))
            vertices.append(Vector((shift+stride, self.length, 0.0)))
            vertices.append(Vector((shift+stride, 0.0, 0.0)))
            speeds += [True, False, False, False]

        create_toolpath_object(self, context, vertices, speeds, "Rect Toolpath")
        return {"FINISHED"}

class DrillToolpathOperator(bpy.types.Operator, AddObjectHelper):
    bl_idname = "mesh.toolpath_add_drill"
    bl_label = "Drill Toolpath"
    bl_options = {"REGISTER", "UNDO"}

    turn_count = FloatProperty(name="Number Turns", description = "How many truns", min=1.0, default=10.0)
    vertex_count = IntProperty(name="Number Vertices", description = "How many vertices per turn", min=3, default=32)
    toolRadius = FloatProperty(name="Tool Radius", description="Radius of the tool", min=0.0, default=0.15)
    screwRadius = FloatProperty(name="Screw Radius", description="Radius of the screw", min=0.0, default=0.5)
    pitch = FloatProperty(name="Pitch", description="Distance between two turns", min=0.0, default=0.1)

    def execute(self, context):
        vertices = []
        speeds = []
        count = int(self.vertex_count*self.turn_count)
        height = count/self.vertex_count*self.pitch
        radius = self.screwRadius-self.toolRadius

        if self.screwRadius <= self.toolRadius:
            vertices += [Vector((0.0, 0.0, -height)), Vector((0.0, 0.0, 0.0))]
            speeds += [True, False]
        else:
            for i in range(0, count):
                param = i/self.vertex_count
                angle = param*pi*2
                vertices.append(Vector((sin(angle)*radius, cos(angle)*radius, -param*self.pitch)))
                speeds.append(True)
            for i in range(0, self.vertex_count+1):
                param = (count+i)/self.vertex_count
                angle = param*pi*2
                vertices.append(Vector((sin(angle)*radius, cos(angle)*radius, -height)))
                if i > 0:
                    speeds.append(True)
            vertices += [Vector((0.0, 0.0, -height)), Vector((0.0, 0.0, 0.0))]
            speeds += [False, False]

        create_toolpath_object(self, context, vertices, speeds, "Drill Toolpath")
        return {"FINISHED"}


def register():
    bpy.utils.register_class(SendToolpathOperator)
    bpy.utils.register_class(RectToolpathOperator)
    bpy.utils.register_class(DrillToolpathOperator)
    #layout.operator(SendToolpathOperator)

def unregister():
    bpy.utils.unregister_class(SendToolpathOperator)
    bpy.utils.unregister_class(RectToolpathOperator)
    bpy.utils.unregister_class(DrillToolpathOperator)
