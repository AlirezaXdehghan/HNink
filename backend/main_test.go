package main

import (
	"bytes"
	"context"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"testing"
	"time"
)

type fixtureTransport map[string]string

func (fixtures fixtureTransport) RoundTrip(request *http.Request) (*http.Response, error) {
	data, ok := fixtures[request.URL.Path]
	if !ok {
		return nil, errors.New("offline")
	}
	return &http.Response{StatusCode: 200, Body: io.NopCloser(strings.NewReader(data))}, nil
}

func TestRefreshPagingAndOfflineCache(t *testing.T) {
	ctx := context.Background()
	fixtures := fixtureTransport{"/v0/topstories.json": "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]"}
	for id := 1; id <= 15; id++ {
		fixtures[fmt.Sprintf("/v0/item/%d.json", id)] = fmt.Sprintf(
			`{"id":%d,"title":"Story %d &amp; notes","by":"alice","score":7,"kids":[50],"url":"https://example.org/story"}`, id, id)
	}
	a := app{dir: t.TempDir(), api: apiBase, client: &http.Client{Transport: fixtures}}
	var output bytes.Buffer
	command := func(args ...string) string {
		t.Helper()
		output.Reset()
		if err := run(ctx, a, &output, args); err != nil {
			t.Fatalf("%v: %v", args, err)
		}
		return output.String()
	}
	command("sync", "top", "1", "6")
	if _, ok := a.getItem(12); !ok {
		t.Fatal("second page was not prefetched")
	}
	if _, ok := a.getItem(13); ok {
		t.Fatal("opening the feed fetched beyond the next page")
	}
	command("hydrate", "top", "3", "6")
	if _, ok := a.getItem(13); !ok {
		t.Fatal("third page did not hydrate")
	}
	command("bookmark", "13")
	command("read", "13")
	delete(fixtures, "/v0/topstories.json")
	if err := run(ctx, a, &output, []string{"sync", "top"}); err == nil {
		t.Fatal("offline refresh should report failure")
	}
	if got := command("view", "top", "3", "6"); strings.Count(got, "\nrow\t") != 3 {
		t.Fatalf("offline feed lost page: %s", got)
	}
	if got := command("view", "top", "1", "14"); strings.Count(got, "\nrow\t") != 14 {
		t.Fatalf("feed ignored a page size above 12: %s", got)
	}
	if got := command("view", "saved"); strings.Count(got, "\nrow\t") != 1 {
		t.Fatalf("bookmark missing offline: %s", got)
	}
	if got, _ := a.getItem(1); got.Title != "Story 1 & notes" {
		t.Fatalf("HTML title decoded incorrectly: %s", got.Title)
	}
}

func TestCommentRepliesAndJobs(t *testing.T) {
	text := strings.Repeat("Keep this sentence. ", 30) + "The final line."
	fixtures := fixtureTransport{
		"/v0/jobstories.json": "[100]",
		"/v0/item/100.json":   `{"id":100,"title":"A job","kids":[101]}`,
		"/v0/item/101.json":   `{"id":101,"by":"bob","text":"First<p>Second","kids":[102]}`,
		"/v0/item/102.json":   fmt.Sprintf(`{"id":102,"by":"alice","text":%q,"kids":[103]}`, text),
		"/v0/item/103.json":   `{"id":103,"by":"chris","text":"Third level","kids":[104]}`,
		"/v0/item/104.json":   `{"id":104,"by":"dana","text":"Fourth level"}`,
	}
	a := app{dir: t.TempDir(), api: apiBase, client: &http.Client{Transport: fixtures}}
	ctx := context.Background()
	for _, args := range [][]string{{"sync", "jobs"}, {"fetch-comments", "100"}} {
		if err := run(ctx, a, io.Discard, args); err != nil {
			t.Fatalf("%v: %v", args, err)
		}
	}
	if _, ok := a.getItem(102); !ok {
		t.Fatal("inline reply preview was not prefetched")
	}
	if _, ok := a.getItem(104); ok {
		t.Fatal("fetched level four before opening its thread")
	}
	a.client.Transport = fixtureTransport{}
	var output bytes.Buffer
	if err := run(ctx, a, &output, []string{"comments", "101"}); err != nil ||
		!strings.Contains(output.String(), "\ncomment\t") {
		t.Fatalf("cached reply missing: %s / %v", output.String(), err)
	}
	if !strings.Contains(output.String(), "\ncontext\t") {
		t.Fatal("opened thread lost the parent comment")
	}
	output.Reset()
	if err := run(ctx, a, &output, []string{"comments", "100"}); err != nil ||
		strings.Count(output.String(), "\ncomment\t") != 3 ||
		!strings.Contains(output.String(), base64.StdEncoding.EncodeToString([]byte(text))) {
		t.Fatalf("cached feed discussion lost its inline reply: %s / %v", output.String(), err)
	}
	if item, _ := a.getItem(101); item.Text != "First\n\nSecond" {
		t.Fatalf("unexpected paragraph text %q", item.Text)
	}
}

func TestReaderTextAndAnchor(t *testing.T) {
	page, _, pages := pageOf([]int{10, 20, 30, 40, 50}, anchorPage([]int{10, 20, 30, 40, 50}, 40, 1, 2), 2)
	if pages != 3 || len(page) != 2 || page[1] != 40 {
		t.Fatalf("anchor moved: %v out of %d pages", page, pages)
	}
	html := "<body><nav>Navigation</nav><article><p>" + strings.Repeat("Reader &amp; reader. ", 12) +
		"</p><script>Script data</script></article></body>"
	content := articleText(html)
	if content == "" || strings.Contains(content, "Navigation") || strings.Contains(content, "Script data") ||
		!strings.Contains(content, "Reader & reader.") {
		t.Fatalf("bad article extraction: %q", content)
	}
	if _, err := webURL("https://someone@example.com/"); err == nil {
		t.Fatal("accepted credentials in publisher URL")
	}
}

func TestDiscussionStreamsEveryRootAndPreservesLinkTarget(t *testing.T) {
	a := app{dir: t.TempDir(), client: &http.Client{Transport: fixtureTransport{}}}
	var kids []int
	for i := 21; i <= 33; i++ {
		kids = append(kids, i)
		if err := write(a.path("items", num(i)), item{ID: i, Text: "A complete comment."}); err != nil {
			t.Fatal(err)
		}
	}
	if err := write(a.path("items", "20"), item{ID: 20, Title: "Discussion", Kids: kids, Fetched: time.Now()}); err != nil {
		t.Fatal(err)
	}
	var result bytes.Buffer
	if err := a.discussion(context.Background(), &result, 20, false); err != nil {
		t.Fatal(err)
	}
	frames := strings.Split(result.String(), "\n.\n")
	if len(frames) < 2 || strings.Count(frames[0], "\ncomment\t") != 13 {
		t.Fatalf("expected all 13 roots in the first cached frame: %q", frames[0])
	}
	got := commentLinks(`<a href="https://example.org/story?x=1&amp;y=2">example.org/...</a> <a href='javascript:alert(1)'>bad</a> <a href="item?id=42">HN</a>`)
	if len(got) != 2 || got[0] != "https://example.org/story?x=1&y=2" || got[1] != "https://news.ycombinator.com/item?id=42" {
		t.Fatalf("incorrect comment link destinations: %v", got)
	}
}
