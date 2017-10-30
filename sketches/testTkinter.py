from Tkinter import *
import ttk

PRECEDENCE = {  "cam_list": 0,
                "sync_list": 1,
                "tri_cams": 2,
                "tri_hmcs": 3,
                "tri_plam": 4 }
NAMES = {   "cam_list": "Cameras",
            "sync_list": "Sync Devices",
            "tri_cams": "Triggered Camcorders",
            "tri_hmcs": "Triggered HMCs",
            "tri_plam": "Triggered PLAMs" }
root = Tk()

tree = ttk.Treeview( root )

for k in PRECEDENCE.keys():
    tree.insert( "", PRECEDENCE[k], k, text=NAMES[k] )

tree.insert( "cam_list", "end", "cam_1", text="Camera 1" )
tree.insert( "cam_list", "end", "cam_2", text="Camera 2" )
tree.insert( "sync_list", 1, text="Sync Box" )

tree.pack()
root.mainloop()