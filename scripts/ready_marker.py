"""Ready protocol metadata. Product versions do not select startup readiness.

Launchers historically check the marker's existence only; they do not parse its
version. Keep that handshake. Python tooling can validate metadata explicitly.
Legacy `version` remains the ready schema, never the product version.
"""
SCHEMA='1.5.0'

def marker(product_version, **fields):
    return {'version':SCHEMA,'readySchemaVersion':SCHEMA,'productVersion':product_version,**fields}

def read(value):
    if not isinstance(value,dict):raise ValueError('Ready marker must be an object')
    legacy=value.get('version');schema=value.get('readySchemaVersion',legacy)
    if legacy is not None and schema!=legacy:raise ValueError('Conflicting ready schema fields')
    if schema!=SCHEMA:raise ValueError('Unsupported or missing ready schema')
    product=value.get('productVersion')
    if product is not None and (not isinstance(product,str) or not product):raise ValueError('Invalid product version')
    return {'readySchemaVersion':schema,'productVersion':product}
