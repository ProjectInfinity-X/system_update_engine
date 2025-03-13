/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.os;

import android.os.IUpdateEngineStableCallback;
import android.os.ParcelFileDescriptor;

/**
 * The stable interface exposed by the update engine daemon.
 *
 * WARNING: this interface exposes less capabilities than IUpdateEngine,
 * for instance, not having a cancel method. This is relied on for
 * security.
 * @hide
 */
interface IUpdateEngineStable {
  /**
   * Apply the given payload as provided in the given file descriptor.
   *
   * See {@link #bind(IUpdateEngineCallback)} for status updates.
   *
   * @param pfd The file descriptor opened at the payload file. Note that the daemon must have
   *   enough permission to operate on the file descriptor.
   * @param payload_offset offset into pfd where the payload binary starts.
   * @param payload_size length after payload_offset to read from pfd. If 0, it will be auto
   *   detected.
   * @param headerKeyValuePairs additional header key value pairs, in the format of "key=value".
   * @see android.os.UpdateEngine#applyPayload(android.content.res.AssetFileDescriptor, String[])
   * @hide
   */
  void applyPayloadFd(in ParcelFileDescriptor pfd,
                      in long payload_offset,
                      in long payload_size,
                      in String[] headerKeyValuePairs);

  /**
   * Bind a callback for status updates on payload application.
   *
   * At any given time, only one callback can be bound. If a callback is already bound,
   * subsequent binding will fail and return false until the bound callback is unbound. That is,
   * binding is first-come, first-serve.
   *
   * A bound callback may be unbound explicitly by calling
   * {@link #unbind(IUpdateEngineStableCallback)}, or
   * implicitly when the process implementing the callback dies.
   *
   * @param callback See {@link IUpdateEngineStableCallback}
   * @return true if binding is successful, false otherwise.
   * @see android.os.UpdateEngine#bind(android.os.UpdateEngineCallback)
   * @hide
   */
  boolean bind(IUpdateEngineStableCallback callback);

  /**
   * Unbind a possibly bound callback.
   *
   * If the provided callback does not match the previously bound callback, unbinding fails.
   *
   * Note that a callback may also be unbound when the process implementing the callback dies.
   * Hence, a client usually does not need to explicitly unbind a callback unless it wants to change
   * the bound callback.
   *
   * @param callback The callback to be unbound. See {@link IUpdateEngineStableCallback}.
   * @return true if unbinding is successful, false otherwise.
   * @see android.os.UpdateEngine#unbind(android.os.UpdateEngineCallback)
   * @hide
   */
  boolean unbind(IUpdateEngineStableCallback callback);


  /**
   * Run postinstall scripts for the given |partition|
   * This allows developers to run postinstall for a partition at
   * a time they see fit. For example, they may wish to run postinstall
   * script when device is IDLE and charging. This method would return
   * immediately if |partition| is empty or does not correspond to any
   * partitions on device. |partition| is expected to be unsuffixed, for
   * example system,product,system_ext, etc.
   * It is allowed to call this function multiple times with the same
   * partition. Postinstall script for that partition would get run more
   * than once. Owners of postinstall scripts should be designed to work
   * correctly in such cases(idempotent). Note this expectation holds even
   * without this API, and it has been so for years.
   * @param Name of thje partition to run postinstall scripts. Should not
   * contain slot suffix.(e.g. system,product,system_ext)
   *
   * @hide
   */
  void triggerPostinstall(in String partition);
}
