def dict_to_obj(d: dict):
    if isinstance(d, dict):
        obj = type("DynamicObj", (), {})()
        for k, v in d.items():
            setattr(obj, k, dict_to_obj(v))
        return obj
    elif isinstance(d, list):
        return [dict_to_obj(x) for x in d]
    else:
        return d
