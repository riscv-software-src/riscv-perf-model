class FieldsValidator:
    @staticmethod
    def validate(object: any, required_fields: list[str] = None, dependent_fields: list[str] = None) -> None:
        if required_fields:
            FieldsValidator.validate_required_fields(object, required_fields)

        if dependent_fields:
            FieldsValidator.validate_dependent_fields(object, dependent_fields)

    @staticmethod
    def validate_required_fields(object: any, required_fields: list[str]) -> None:
        if isinstance(required_fields, list):
            for field in required_fields:
                if field not in object:
                    raise KeyError(f"Missing required field: {field}")
            return

        for field, sub_fields in required_fields.items():
            if field not in object or object.get(field) is None:
                raise KeyError(f"Missing required field: {field}")
            FieldsValidator.validate_required_fields(
                object.get(field), sub_fields)

    @staticmethod
    def validate_dependent_fields(object: any, dependent_fields: list[str]) -> None:
        if isinstance(dependent_fields, list):
            fields_count = 0
            for field in dependent_fields:
                if field in object:
                    fields_count += 1
            if fields_count < len(dependent_fields):
                raise ValueError(f"Object is incomplete: fields {dependent_fields} are interdependent and must either all be present or all be omitted.")
            return

        for field, sub_fields in dependent_fields.items():
            if field in object and object.get(field) is not None:
                FieldsValidator.validate_dependent_fields(
                    object.get(field), sub_fields)
