package main

import (
	"context"
	"encoding/xml"
	"flag"
	"fmt"
	"os"

	"github.com/BurntSushi/toml"
	"github.com/EYERCORD/deepl-sdk-go"
	"github.com/EYERCORD/deepl-sdk-go/params"
	"github.com/EYERCORD/deepl-sdk-go/types"
	dpl "github.com/cluttrdev/deepl-go/deepl"
)

type Config struct {
	DeepL struct {
		APIKey string `toml:"api_key"`
	} `toml:"deepl"`
	OpenAI struct {
		APIKey string `toml:"api_key"`
	} `toml:"openai"`
}

type TS struct {
	XMLName  xml.Name  `xml:"TS"`
	Version  string    `xml:"version,attr"`
	Language string    `xml:"language,attr"`
	Context  []Context `xml:"context"`
}

type Context struct {
	Name    string    `xml:"name"`
	Message []Message `xml:"message"`
}

type Message struct {
	Source      string      `xml:"source"`
	Translation Translation `xml:"translation"`
}

type Translation struct {
	Type  string `xml:"type,attr,omitempty"`
	Value string `xml:",chardata"`
}

var (
	config Config
)

func loadConfig(configFile string) error {
	_, err := toml.DecodeFile(configFile, &config)
	return err
}

func translateWithDeepL(text, targetLang string) (string, error) {
	translator, err := dpl.NewTranslator(config.DeepL.APIKey)
	if err != nil {
		return "", err
	}

	opts := []dpl.TranslateOption{}
	opts = append(opts, dpl.WithSourceLang("EN"))
	opts = append(opts, dpl.WithPreserveFormatting(true))

	fmt.Printf("Translating: \"%s\"   --->   ", text)

	translations, err := translator.TranslateText([]string{text}, targetLang, opts...)
	if err != nil {
		fmt.Printf("Failed\n")
		return "", err
	}

	fmt.Printf("\"%s\"\n", translations[0].Text)

	return translations[0].Text, nil
}

func translateWithDeepL2(text, targetLang string) (string, error) {
	client, err := deepl.NewClient(config.DeepL.APIKey, "free")
	if err != nil {
		fmt.Println(err)
		return "", err
	}

	t := []string{text}
	params := &params.TranslateTextParams{
		TargetLang:         types.TargetLangEN,
		Text:               t,
		PreserveFormatting: types.PreserveFormattingEnabled,
		SourceLang:         types.SourceLangEN,
	}

	switch targetLang {
	case "de_DE":
		params.TargetLang = types.TargetLangDE
	case "fr_FR":
		params.TargetLang = types.TargetLangFR
	case "it_IT":
		params.TargetLang = types.TargetLangIT
	case "es_ES":
		params.TargetLang = types.TargetLangES
	}

	fmt.Printf("Translating: \"%s\"   --->   ", text)

	res, errRes, err := client.TranslateText(context.TODO(), params)
	if err != nil {
		fmt.Println(err)
	}

	if errRes != nil {
		fmt.Println("ErrorResponse", errRes.Message)
	}

	fmt.Printf("\"%s\"\n", res.Translations[0].Text)

	return res.Translations[0].Text, nil
}

func translateWithGPT(text, targetLang string) (string, error) {
	return "", fmt.Errorf("GPT-3 translation not implemented yet")
}

func main() {
	inputFile := flag.String("input", "", "Translation file to process (.ts)")
	outputFile := flag.String("output", "output.ts", "Output file to write the translated content")
	useDeepL := flag.Bool("deepl", false, "Use DeepL API for translation (default is OpenAI GPT-3)")
	removeUnfinished := flag.Bool("remove-unfinished", false, "Remove unfinished translations flag from the output file")
	configFile := flag.String("config", "auto_translate.conf", "Fichier de configuration TOML")
	flag.Parse()

	err := loadConfig(*configFile)
	if err != nil {
		fmt.Printf("Error loading configuration: %v\n", err)
		return
	}

	if *inputFile == "" {
		fmt.Println("Input file is required")
		return
	}

	content, err := os.ReadFile(*inputFile)
	if err != nil {
		fmt.Printf("Error reading input file: %v\n", err)
		return
	}

	var ts TS
	err = xml.Unmarshal(content, &ts)
	if err != nil {
		fmt.Printf("Error parsing XML: %v\n", err)
		return
	}

	for i, context := range ts.Context {
		for j, message := range context.Message {
			if message.Translation.Type == "unfinished" {
				var translation string
				if *useDeepL {
					translation, err = translateWithDeepL2(message.Source, ts.Language)
				} else {
					translation, err = translateWithGPT(message.Source, ts.Language)
				}
				if err != nil {
					fmt.Printf("Error translating text: %v\n", err)
					return
				}
				ts.Context[i].Message[j].Translation.Value = translation
				if *removeUnfinished {
					ts.Context[i].Message[j].Translation.Type = ""
				}
			}
		}
	}

	output, err := xml.MarshalIndent(ts, "", "  ")
	if err != nil {
		fmt.Printf("Failed to generate XML: %v\n", err)
		return
	}

	err = os.WriteFile(*outputFile, output, 0644)
	if err != nil {
		fmt.Printf("Failed to write outpuot file: %v\n", err)
		return
	}

	fmt.Println("Translation completed successfully.")
}
