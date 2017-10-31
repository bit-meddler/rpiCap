from Tkinter import *
import ttk
# import OpenGL.Tk

ORDER = {   "cam_list":  0,
            "syn_list": 10,
            "tri_cams": 200,
            "tri_hmcs": 300,
            "tri_plam": 400 }
NAMES = {   "cam_list": "Cameras",
            "syn_list": "Sync Devices",
            "tri_cams": "Triggered Camcorders",
            "tri_hmcs": "Triggered HMCs",
            "tri_plam": "Triggered PLAMs" }
root = Tk()

tree = ttk.Treeview( root, show=['tree'] )

TAG_DEF  = 0x0
TAG_SEL  = 0x1
TAG_DIS  = 0x2

def onSelect( event ):
    # not good, only a test
    global tree
    sel_list = tree.selection()
    print sel_list
    for itm in sel_list:
        old_tag = tree.item( itm, "tag" )[0]
        # Bit Twiddle!
        
tree.tag_configure( TAG_DEF,         font=('Helvetica', 10) )
tree.tag_configure( TAG_DIS,         font=('Helvetica', 10, 'italic') )
tree.tag_configure( TAG_SEL,         font=('Helvetica', 10, 'bold') )
tree.tag_configure( TAG_DIS|TAG_SEL, font=('Helvetica', 10, 'bold italic') )


for k in ORDER.keys():
    tree.insert( "", ORDER[k], k, text=NAMES[k], tag=TAG_DEF )

tree.insert( "cam_list", "end", "cam_1", text="Camera 1", tag=TAG_DEF )
tree.insert( "cam_list", "end", "cam_2", text="Camera 2", tag=TAG_DEF )
tree.insert( "cam_list", "end", "cam_3", text="Camera 3", tag=TAG_DEF )
tree.insert( "cam_list", "end", "cam_4", text="Camera 4", tag=TAG_DEF )

tree.insert( "syn_list", 1, "sync_1", text="Sync Box", tag=TAG_DEF )

# make unpopulated headings italic
tree.item( "tri_cams", tag=TAG_DIS )
tree.item( "tri_hmcs", tag=TAG_DIS )
tree.item( "tri_plam", tag=TAG_DIS )

tree.bind(" <ButtonRelease-1>", onSelect )

tree.pack()
root.mainloop()