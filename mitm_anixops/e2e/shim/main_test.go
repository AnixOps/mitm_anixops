package main

import (
	"sync"
	"testing"
	"time"
)

func TestEngineWithCoreHoldsMutexForPolicyCoreCall(t *testing.T) {
	core := &engine{}
	entered := make(chan struct{})
	release := make(chan struct{})
	done := make(chan struct{})
	var releaseOnce sync.Once
	releaseCore := func() {
		releaseOnce.Do(func() {
			close(release)
		})
	}
	defer releaseCore()

	go func() {
		core.withCore(func() {
			close(entered)
			<-release
		})
		close(done)
	}()

	select {
	case <-entered:
	case <-time.After(time.Second):
		t.Fatal("policy-core call did not enter the serialized region")
	}

	if core.mu.TryLock() {
		core.mu.Unlock()
		t.Fatal("policy-core mutex was not held for the complete call")
	}

	releaseCore()
	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("policy-core call did not leave the serialized region")
	}
}
