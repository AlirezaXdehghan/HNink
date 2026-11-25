package main

import (
	"bytes"
	"context"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"strings"
	"sync/atomic"
	"testing"
)

type accountTestTransport func(*http.Request) (*http.Response, error)

func (f accountTestTransport) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }

func TestHNAccountPersistsSessionAndAddsFavoriteOnce(t *testing.T) {
	const username, password, sessionCookie = "read-er", "a + %/& password", "session-cookie"
	var favorites atomic.Int32
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/login" {
			if r.Method != http.MethodPost || r.FormValue("acct") != username || r.FormValue("pw") != password {
				http.Error(w, "bad login", http.StatusUnauthorized)
				return
			}
			http.SetCookie(w, &http.Cookie{Name: "user", Value: sessionCookie, HttpOnly: true, Path: "/"})
			w.Header().Set("Location", "news")
			w.WriteHeader(http.StatusFound)
			return
		}
		cookie, err := r.Cookie("user")
		if err != nil || cookie.Value != sessionCookie {
			io.WriteString(w, `<a href="login?goto=item">login</a>`)
			return
		}
		switch r.URL.Path {
		case "/item":
			if favorites.Load() == 0 {
				io.WriteString(w, `<a href="https://other.invalid/fave?id=23&amp;auth=wrong">favorite</a>
					<a href="fave?id=24&amp;auth=wrong">favorite</a>
					<a href="fave?id=23&amp;auth=correct&amp;goto=item%3Fid%3D23">favorite</a>`)
			} else {
				io.WriteString(w, `<a href="fave?id=23&amp;auth=correct&amp;un=t">unfavorite</a>`)
			}
		case "/fave":
			if r.URL.Query().Get("id") != "23" || r.URL.Query().Get("auth") != "correct" {
				http.Error(w, "bad favorite", http.StatusBadRequest)
				return
			}
			favorites.Add(1)
			w.WriteHeader(http.StatusFound)
		default:
			http.NotFound(w, r)
		}
	})

	a := newHNAccount()
	a.dir = t.TempDir()
	a.client.Transport = accountTestTransport(func(r *http.Request) (*http.Response, error) {
		w := httptest.NewRecorder()
		handler.ServeHTTP(w, r)
		return w.Result(), nil
	})
	var result bytes.Buffer
	run := func(args []string, credentials string) string {
		t.Helper()
		result.Reset()
		if !runAccount(context.Background(), a, &result, strings.NewReader(credentials), args) {
			t.Fatalf("unhandled command %v", args)
		}
		return result.String()
	}
	if got := run([]string{"login"}, username+"\nbad\n"); !strings.HasPrefix(got, "error\t") {
		t.Fatalf("bad login was accepted: %q", got)
	}
	if got := run([]string{"login"}, username+"\n"+password+"\n"); !strings.HasPrefix(got, "account\t") {
		t.Fatalf("login failed: %q", got)
	}
	session, ok := a.session()
	if !ok || session.Username != username || session.Cookie != sessionCookie {
		t.Fatal("new account invocation could not load the session")
	}
	stored, err := os.ReadFile(a.sessionPath())
	if err != nil || bytes.Contains(stored, []byte(password)) {
		t.Fatal("session file is missing or contains the password")
	}
	info, err := os.Stat(a.sessionPath())
	if err != nil || info.Mode().Perm() != 0600 {
		t.Fatal("session file is accessible to other users")
	}
	for range 2 {
		if got := run([]string{"favorite", "23"}, ""); !strings.HasPrefix(got, "favorite\t") {
			t.Fatalf("favorite request failed: %q", got)
		}
	}
	if favorites.Load() != 1 || !strings.HasPrefix(run([]string{"favorite-status", "23"}, ""), "favorite\t") {
		t.Fatal("favorite action was duplicated or its state was not saved")
	}
	run([]string{"logout"}, "")
	if _, ok := a.session(); ok {
		t.Fatal("logout left the session available")
	}
}
