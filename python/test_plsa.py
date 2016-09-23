import plsa
import numpy as np
import json
import argparse

TO_REPLACE = [",", ".", "!", "?", ";", "(", ")"]

def read_json(filename, vocab = None):
	if vocab is None:
		vocab = dict()
		word_list = []
		can_add = True
	else:
		can_add = False

	words = []
	docs = []
	weights = []
	with open(filename, "r") as f:
		data = json.load(f)
		doc_keys = sorted(data.keys())
		for doc_idx, doc_key in enumerate(doc_keys):
			doc = data[doc_key]
			freq = dict()
			print "Processing document `%s'..." % doc_key

			for t in TO_REPLACE:
				doc = doc.replace(t, " ")
			doc = doc.lower().encode("utf-8")

			content = doc.split()
			for word in content:
				if not word in vocab:
					if not can_add:
						raise ValueError("vocabulary")
					vocab[word] = len(vocab)
					word_list.append(word)
				word_idx = vocab[word]
				if not word_idx in freq:
					freq[word_idx] = 0
				freq[word_idx] += 1

			for word_idx in freq.iterkeys():
				docs.append(doc_idx)
				words.append(word_idx)
				weights.append(freq[word_idx])

	num_docs = len(doc_keys)
	num_words = len(vocab)
	np_words = np.array(words, dtype = np.int32)
	np_docs = np.array(docs, dtype = np.int32)
	np_weights = np.array(weights, dtype = np.float)
	if can_add:
		return num_docs, num_words, vocab, word_list, \
		       np_words, np_docs, np_weights
	else:
		return num_docs, np_words, np_docs, np_weights

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument("--train_file", required = True,
	                    help = "Name of the training file")
	parser.add_argument("--test_file", required = True,
	                    help = "Name of the test file")
	parser.add_argument("--num_topics", type = int, default = 40,
	                    help = "Number of topics to print")
	parser.add_argument("--top_words", type = int, default = 30,
	                    help = "Number of words per topic")
	parser.add_argument("--top_topics", type = int, default = 5,
	                    help = "Number of topics per document")
	parser.add_argument("--tol", type = float, default = 1e-3,
	                    help = "Convergence tolerance")
	parser.add_argument("--max_iter", type = int, default = 1000,
	                    help = "Maximum number of iterations")
	args = parser.parse_args()

	tol = float(args.tol)
	max_iter = int(args.max_iter)
	num_topics = int(args.num_topics)
	top_words = int(args.top_words)
	top_topics = int(args.top_topics)
	num_docs, num_words, vocab, word_list, words, docs, weights = \
	    read_json(args.train_file)

	DT, TW = plsa.plsa_train(words, docs, weights, num_words,
	                         num_docs, num_topics, tol, max_iter)

	for topic in xrange(num_topics):
		print "\n\nTopic %d:" % (topic + 1)
		s = np.argsort(TW[topic, :])[::-1]
		for i in xrange(top_words):
			print "%s: %g" % (word_list[s[i]], TW[topic, s[i]])

	num_docs_test, words_test, docs_test, weights_test = \
	    read_json(args.test_file, vocab)

	DT_test = plsa.plsa_retrain(words_test, docs_test, weights_test,
	                            num_docs_test, TW, tol, max_iter)

	for doc in xrange(num_docs_test):
		print "\n\nTest document %d..." % (doc + 1)
		s = np.argsort(DT_test[doc, :])[::-1]
		for i in xrange(top_topics):
			print "%d: %g" % (s[i] + 1, DT_test[doc, s[i]])
