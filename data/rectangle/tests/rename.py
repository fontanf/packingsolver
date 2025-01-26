import pathlib

pathlist = pathlib.Path(".").glob('**/*.csv')
for p in pathlist:
    if "_items.csv" in p.name:
        p.rename(pathlib.Path(p.parent, "items.csv"))
    elif "_bins.csv" in p.name:
        p.rename(pathlib.Path(p.parent, "bins.csv"))
    elif "_defects.csv" in p.name:
        p.rename(pathlib.Path(p.parent, "defects.csv"))
    elif "_parameters.csv" in p.name:
        p.rename(pathlib.Path(p.parent, "parameters.csv"))
    elif "_solution.csv" in p.name:
        p.rename(pathlib.Path(p.parent, "solution.csv"))
    elif "README" in p.name or "readme" in p.name:
        p.rename(pathlib.Path(p.parent, "README.md"))
