    } else {
        if (!EC_POINT_copy(s, point))
            goto err;
    }

    EC_POINT_BN_set_flags(s, BN_FLG_CONSTTIME);

    cardinality = BN_CTX_get(ctx);
    lambda = BN_CTX_get(ctx);
    k = BN_CTX_get(ctx);
    if (k == NULL || !BN_mul(cardinality, group->order, group->cofactor, ctx))
        goto err;

    /*
     * Group cardinalities are often on a word boundary.
     * So when we pad the scalar, some timing diff might
     * pop if it needs to be expanded due to carries.
     * So expand ahead of time.
     */
    cardinality_bits = BN_num_bits(cardinality);
    group_top = bn_get_top(cardinality);
    if ((bn_wexpand(k, group_top + 2) == NULL)
        || (bn_wexpand(lambda, group_top + 2) == NULL))
        goto err;

    if (!BN_copy(k, scalar))
        goto err;

    BN_set_flags(k, BN_FLG_CONSTTIME);

    if ((BN_num_bits(k) > cardinality_bits) || (BN_is_negative(k))) {
        /*-
         * this is an unusual input, and we don't guarantee
         * constant-timeness
         */
        if (!BN_nnmod(k, k, cardinality, ctx))
            goto err;
    }
    if ((BN_num_bits(k) > cardinality_bits) || (BN_is_negative(k))) {
        /*-
         * this is an unusual input, and we don't guarantee
         * constant-timeness
         */
        if (!BN_nnmod(k, k, cardinality, ctx))
            goto err;
    }

    if (!BN_add(lambda, k, cardinality))
        goto err;
    BN_set_flags(lambda, BN_FLG_CONSTTIME);
    if (!BN_add(k, lambda, cardinality))
        goto err;
    /*
     * lambda := scalar + cardinality
     * k := scalar + 2*cardinality
     */
    kbit = BN_is_bit_set(lambda, cardinality_bits);
    BN_consttime_swap(kbit, k, lambda, group_top + 2);

    group_top = bn_get_top(group->field);
    if ((bn_wexpand(s->X, group_top) == NULL)
        || (bn_wexpand(s->Y, group_top) == NULL)
        || (bn_wexpand(s->Z, group_top) == NULL)
        || (bn_wexpand(r->X, group_top) == NULL)
        || (bn_wexpand(r->Y, group_top) == NULL)
        || (bn_wexpand(r->Z, group_top) == NULL))
        goto err;

    /* top bit is a 1, in a fixed pos */
    if (!EC_POINT_copy(r, s))
        goto err;

    EC_POINT_BN_set_flags(r, BN_FLG_CONSTTIME);

    if (!EC_POINT_dbl(group, s, s, ctx))
        goto err;

    pbit = 0;

#define EC_POINT_CSWAP(c, a, b, w, t) do {         \
        BN_consttime_swap(c, (a)->X, (b)->X, w);   \
        BN_consttime_swap(c, (a)->Y, (b)->Y, w);   \
        BN_consttime_swap(c, (a)->Z, (b)->Z, w);   \
        t = ((a)->Z_is_one ^ (b)->Z_is_one) & (c); \
        (a)->Z_is_one ^= (t);                      \
        (b)->Z_is_one ^= (t);                      \
} while(0)

    /*-
     * The ladder step, with branches, is
     *
     * k[i] == 0: S = add(R, S), R = dbl(R)
     * k[i] == 1: R = add(S, R), S = dbl(S)
     *
     * Swapping R, S conditionally on k[i] leaves you with state
     *
     * k[i] == 0: T, U = R, S
     * k[i] == 1: T, U = S, R
     *
     * Then perform the ECC ops.
     *
     * U = add(T, U)
     * T = dbl(T)
     *
     * Which leaves you with state
     *
     * k[i] == 0: U = add(R, S), T = dbl(R)
     * k[i] == 1: U = add(S, R), T = dbl(S)
     *
     * Swapping T, U conditionally on k[i] leaves you with state
     *
     * k[i] == 0: R, S = T, U
     * k[i] == 1: R, S = U, T
     *
     * Which leaves you with state
     *
     * k[i] == 0: S = add(R, S), R = dbl(R)
     * k[i] == 1: R = add(S, R), S = dbl(S)
     *
     * So we get the same logic, but instead of a branch it's a
     * conditional swap, followed by ECC ops, then another conditional swap.
     *
     * Optimization: The end of iteration i and start of i-1 looks like
     *
     * ...
     * CSWAP(k[i], R, S)
     * ECC
     * CSWAP(k[i], R, S)
     * (next iteration)
     * CSWAP(k[i-1], R, S)
     * ECC
     * CSWAP(k[i-1], R, S)
     * ...
     *
     * So instead of two contiguous swaps, you can merge the condition
     * bits and do a single swap.
     *
     * k[i]   k[i-1]    Outcome
     * 0      0         No Swap
     * 0      1         Swap
     * 1      0         Swap
     * 1      1         No Swap
     *
     * This is XOR. pbit tracks the previous bit of k.
     */

    for (i = cardinality_bits - 1; i >= 0; i--) {
        kbit = BN_is_bit_set(k, i) ^ pbit;
        EC_POINT_CSWAP(kbit, r, s, group_top, Z_is_one);
        if (!EC_POINT_add(group, s, r, s, ctx))
            goto err;
        if (!EC_POINT_dbl(group, r, r, ctx))
            goto err;
        /*
         * pbit logic merges this cswap with that of the
         * next iteration
         */
        pbit ^= kbit;
    }