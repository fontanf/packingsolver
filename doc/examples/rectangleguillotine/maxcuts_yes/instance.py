import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.set_number_of_stages(2)
instance_builder.set_maximum_number_1_cuts(1)
instance_builder.add_bin_type(10, 10, copies=3)
instance_builder.add_item_type(3, 10, oriented=True, copies=3)
instance = instance_builder.build()
