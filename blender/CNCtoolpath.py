import bpy
import bmesh

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
    resStr = '{\n\t"type": "polygon",\n\t"speed": '+floatFormat.format(speed)+',\n\t"vertices": ['
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
        if prevVertex != None:
            resStr += ','
            if len(vertex.link_edges) > 2:
                break
        pos = vertex.co*scale
        resStr += '\n\t\t['+floatFormat.format(-pos.y)+','+floatFormat.format(-pos.x)+','+floatFormat.format(pos.z)+']'
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
    return resStr+'\n\t]\n}\n'

class ExportToolpathOperator(bpy.types.Operator):
    bl_idname = "export_scene.export_toolpath"
    bl_label = "Export Toolpath"
    filter_glob = bpy.props.StringProperty(default="*.json", options={'HIDDEN'})
    filepath = bpy.props.StringProperty(subtype="FILE_PATH")
    scaleFactor = bpy.props.FloatProperty(name="Scale", min=0.001, max=1000.0, default=1.0)

    @classmethod
    def poll(cls, context):
        return context.scene.objects.active is not None

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        outFile = open(bpy.path.ensure_ext(self.filepath, ".json"), "w")
        outFile.write(ToolpathToJSON(context.scene.objects.active, self.scaleFactor, 1.0))
        outFile.close()
        return {'FINISHED'}

def ExportMenu(self, context):
    self.layout.operator_context = 'INVOKE_DEFAULT'
    self.layout.operator(ExportToolpathOperator.bl_idname, text="CNC Toolpath (.json)")

def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_export.append(ExportMenu)

def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(ExportMenu)

if __name__ == "__main__":
    register()
    bpy.ops.export_scene.export_toolpath()
