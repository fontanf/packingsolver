import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.set_first_stage_orientation(psg.CutOrientation.Vertical)
instance_builder.add_bin_type(20, 10, copies=3)
instance_builder.add_item_type(10, 3, oriented=True)
instance_builder.add_item_type(10, 3, oriented=True)
instance_builder.add_item_type(10, 4, oriented=True)
instance_builder.add_item_type(10, 10, oriented=True)
instance = instance_builder.build()
