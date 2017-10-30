from Tkinter import *
import ttk

ORDER = {   "cam_list": 0,
            "syn_list": 1,
            "tri_cams": 2,
            "tri_hmcs": 3,
            "tri_plam": 4 }
NAMES = {   "cam_list": "Cameras",
            "syn_list": "Sync Devices",
            "tri_cams": "Triggered Camcorders",
            "tri_hmcs": "Triggered HMCs",
            "tri_plam": "Triggered PLAMs" }
root = Tk()

tree = ttk.Treeview( root, show=['tree'] )

tree.tag_configure( "default",  font=('Helvetica', 10) )
tree.tag_configure( "disabled", font=('Helvetica', 10, 'italic') )

for k in ORDER.keys():
    tree.insert( "", ORDER[k], k, text=NAMES[k], tag="default" )

tree.insert( "cam_list", "end", "cam_1", text="Camera 1", tag="default" )
tree.insert( "cam_list", "end", "cam_2", text="Camera 2", tag="default" )
tree.insert( "cam_list", "end", "cam_3", text="Camera 3", tag="default" )
tree.insert( "cam_list", "end", "cam_4", text="Camera 4", tag="default" )

tree.insert( "syn_list", 1, text="Sync Box", tag="default" )

# make unpopulated headings italic
tree.item( "tri_cams", tag="disabled" )
tree.item( "tri_hmcs", tag="disabled" )
tree.item( "tri_plam", tag="disabled" )

tree.pack()
root.mainloop()