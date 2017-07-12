import bpy

def select(i):
    for ob in bpy.context.selected_objects:
        ob.select = False

    bpy.data.objects['cam_'+str(i)].select = True
    bpy.data.objects['light_'+str(i)].select = True
    bpy.data.objects['conn_'+str(i)].select = True

    bpy.context.scene.objects.active = bpy.data.objects['conn_'+str(i)]

def select_exc(i):
    for ob in bpy.context.selected_objects:
        ob.select = False

    for ob in bpy.data.objects:
        if ob.name.startswith('cam_') or ob.name.startswith('light_') or ob.name.startswith('conn_'):
            ob.hide = True

    bpy.data.objects['cam_'+str(i)].select = True
    bpy.data.objects['cam_'+str(i)].hide = False

    bpy.data.objects['light_'+str(i)].select = True
    bpy.data.objects['light_'+str(i)].hide = False

    bpy.data.objects['conn_'+str(i)].select = True
    bpy.data.objects['conn_'+str(i)].hide = False

    bpy.context.scene.objects.active = bpy.data.objects['conn_'+str(i)]

def clear():
    for ob in bpy.context.selected_objects:
        ob.select = False

    for ob in bpy.data.objects:
        if ob.name.startswith('cam_') or ob.name.startswith('light_') or ob.name.startswith('conn_'):
            ob.select = True
    bpy.ops.object.delete();