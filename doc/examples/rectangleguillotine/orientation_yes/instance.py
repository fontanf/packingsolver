import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPacking)
instance_builder.set_number_of_stages(2)
instance_builder.set_first_stage_orientation(psg.CutOrientation.Vertical)
instance_builder.add_bin_type(10, 10, copies=3)
instance_builder.add_item_type(4, 3, oriented=True)
instance_builder.add_item_type(4, 7, oriented=True)
instance_builder.add_item_type(6, 5, oriented=True)
instance_builder.add_item_type(6, 4, oriented=True)
instance = instance_builder.build()
