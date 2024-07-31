# Check the effect of path pre-validation

When migrating to a new path, QUIC limits the sending speed to avoid
amplification attacks.  This can be avoided by pre-validation, i.e.,
validating the new path (address, connection ID) before migrating to
it.  The RFC supports it, the Quinn implementation does not.

When `validation==skip`, we emulate pre-validation by not validating
the new path at all.

Relies on https://github.com/nemethf/quinn/tree/pre-validation

