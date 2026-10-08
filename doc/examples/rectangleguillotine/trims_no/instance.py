import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.add_bin_type(10, 10, copies=3)
instance_builder.add_item_type(4, 8, oriented=True)
instance_builder.add_item_type(3, 8, oriented=True)
instance_builder.add_item_type(3, 8, oriented=True)
instance = instance_builder.build()
