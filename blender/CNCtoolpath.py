import bpy
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

def ToolpathToJSON(obj, scale, speed):
    result = {"type": "polygon", "speed":speed, "vertices":list()}

    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    mesh.verts.ensure_lookup_table()
    mesh.faces.ensure_lookup_table()
    if len(mesh.faces) != 1:
        return

    vertex = None
    for eI, vert in enumerate(mesh.faces[0].verts):
        if len(vert.link_edges) == 3:
            if vertex == None:
                vertex = vert
            else:
                return

    prevVertex = None
    while True:
        if prevVertex != None and len(vertex.link_edges) > 2:
            break
        pos = vertex.co*scale
        result["vertices"].append((-pos.y, -pos.x, pos.z))
        if len(vertex.link_edges) < 2:
            break
        for eI, edge in enumerate(vertex.link_edges):
            vert = edge.other_vert(vertex)
            if vert != prevVertex and len(vert.link_faces) == 0:
                prevVertex = vertex
                vertex = vert
                break

    mesh.to_mesh(obj.data)
    mesh.free()
    return result

class SendToolpathOperator(bpy.types.Operator):
    bl_idname = "object.send_toolpath"
    bl_label = "Send Toolpath"
    address = bpy.props.StringProperty(name="IP-Address", default="10.0.1.10")
    port = bpy.props.StringProperty(name="Port", default="3823")
    scale = bpy.props.StringProperty(name="Scale", default="1.0")
    speed = bpy.props.StringProperty(name="Speed", default="1.0")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        packer = msgpack.Packer()
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect((self.address, int(self.port)))
        client.send(packer.pack(ToolpathToJSON(context.active_object, float(self.scale), float(self.speed))))
        client.close()
        return {"FINISHED"}

def register():
    bpy.utils.register_class(SendToolpathOperator)
    #layout.operator(SendToolpathOperator)

def unregister():
    bpy.utils.unregister_class(SendToolpathOperator)
