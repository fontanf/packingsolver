import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.add_bin_type(10, 12, copies=3)
instance_builder.add_item_type(10, 6, oriented=True, copies=2)
instance = instance_builder.build()
