/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
package org.opensolaris.os.dtrace;

import java.util.*;

/**
 * A value generated by the DTrace {@code stack()}, {@code ustack()}, or
 * {@code jstack()} action.
 *
 * @author Tom Erickson
 */
public interface StackValueRecord extends ValueRecord {
    /**
     * Gets a copy of this record's stack frames, or an empty array if
     * this record's raw stack data was not converted to human-readable
     * stack frames by DTrace.  Raw stack data is not converted (i.e.
     * human-readable stack frames are omitted) whenever a {@code
     * printa()} format string is specified without including the {@code
     * %k} placeholder for the stack value represented by this record.
     * (The {@code stack()}, {@code ustack()}, and {@code jstack()}
     * actions are all usable as members of an aggregation tuple.)  See
     * the <a
     * href=http://dtrace.org/guide/chp-fmt.html#chp-fmt-printa>
     * <b>{@code printa()}</b></a> section of the <b>Output
     * Formatting</b> chapter of the <i>Dynamic Tracing
     * Guide</i> for details about {@code printa()} format strings.
     * Human-readable stack frames are generated by default if {@code
     * printa()} is called without specifying a format string, or when
     * using {@link Consumer#getAggregate()} as an alternative to {@code
     * printa()}.  They are also generated when {@code stack()}, {@code
     * ustack()}, or {@code jstack()} is used as a stand-alone action
     * outside of an aggregation tuple.
     * <p>
     * The returned array is a copy and modifying it has no effect on
     * this record.  Elements of the returned array are not {@code
     * null}.
     *
     * @return a non-null, possibly empty array of this record's
     * human-readable stack frames, none of which are {@code null}
     */
    public StackFrame[] getStackFrames();

    /**
     * Gets the native DTrace representation of this record's stack as
     * an array of raw bytes.  The raw bytes are needed to distinguish
     * stacks that have the same string representation but are
     * considered distinct by DTrace.  Duplicate stacks (stacks with the
     * same human-readable stack frames) can have distinct raw stack
     * data when program text is relocated.
     * <p>
     * Implementations of this interface use raw stack data to compute
     * {@link Object#equals(Object o) equals()} and {@link
     * Object#hashCode() hashCode()}.  If the stack belongs to a user
     * process, the raw bytes include the process ID.
     *
     * @return the native DTrace library's internal representation of
     * this record's stack as a non-null array of bytes
     */
    public byte[] getRawStackData();

    /**
     * Gets the raw bytes used to represent this record's stack value in
     * the native DTrace library.
     *
     * @return {@link #getRawStackData()}
     */
    public Object getValue();

    /**
     * Gets a read-only {@code List} view of this record's stack frames.
     * The returned list implements {@link java.util.RandomAccess}.  It
     * is empty if {@link #getStackFrames()} returns an empty array.
     *
     * @return non-null, unmodifiable {@code List} view of this record's
     * stack frames
     */
    public List <StackFrame> asList();
}
